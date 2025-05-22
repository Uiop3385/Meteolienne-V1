#include <LiquidCrystal_I2C.h>
#include <neotimer.h>
#include <ScreenUi.h>
#include <Keypad.h>
#include <Stepper.h>
#include <EEPROM-Storage.h>
#include "ProgressBar.h"

// improved Input class
class ImprovedInput : public Input {
public:
    ImprovedInput(char *text) : Input(text) {}

    const char* getInputText() {
        return text_;
    }
};

// controls definition
#define KEY_UP '2'
#define KEY_DOWN '8'
#define KEY_LEFT '4'
#define KEY_RIGHT '6'
#define KEY_CANCEL '*'
#define KEY_ENTER '#'
#define KEY_A 'A'
#define KEY_B 'B'
#define KEY_C 'C'
#define KEY_D 'D'

// custom characters
const byte girouette_symbol[8] = {0b00000,0b00000,0b00000,0b00011,0b11111,0b00100,0b00100,0b00000};
const byte a_grave_accent[8] = {0b01000,0b00100,0b00000,0b01110,0b00001,0b01111,0b10001,0b01111};
const byte e_acute_accent[8] = {0b00010,0b00100,0b00000,0b01110,0b10001,0b11111,0b10000,0b01110};
const byte e_grave_accent[8] = {0b01000,0b00100,0b00000,0b01110,0b10001,0b11111,0b10000,0b01110};
const byte c_cedilla[8] = {0b00000,0b01110,0b10000,0b10000,0b10001,0b01110,0b00100,0b01100};
const byte degree[8] = {0b01100,0b10010,0b10010,0b01100,0b00000,0b00000,0b00000,0b00000};

// stepper variables
#define steps_per_revolution 200
int16_t current_step = 1;
int16_t previous_step = 1;
Stepper stepper(steps_per_revolution, 13, 12, 11, 10);

// girouette variables
#define girouette_pin A1
float angle_values[] = {0, 22.5, 45, 67.5, 90, 112.5, 135, 157.5, 180, 202.5, 225, 247.5, 270, 292.5, 315, 337.5};
int voltage_values[] = {788, 705, 889, 830, 946, 602, 633, 246, 92, 126, 185, 65, 289, 83, 464, 408};
float girouette_direction = 0;
float previous_direction = 0;

// keypad variables
#define keypad_rows 4
#define keypad_columns 4
const char keymap[keypad_rows][keypad_columns] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
const byte row_pins[keypad_rows] = {22, 23, 24, 25};
const byte column_pins[keypad_columns] = {26, 27, 28, 29};
Keypad keypad = Keypad(makeKeymap(keymap), row_pins, column_pins, keypad_rows, keypad_columns);
char key_pressed;

// code variables
#define password_length 5
String master = "C81A";
bool locked = true;
String data;
byte data_count = 0;

// settings variables
bool backlight = true;
#define input_length 9
char input[input_length] = "Bonjour!";
uint8_t list_value = 0;
uint8_t arrondi = 1;

// screen variables
LiquidCrystal_I2C LCD(0x27, 20, 4);
ProgressBar reset_progress(LCD, 13, 3, 0);
String current_screen = "main_menu";

// status LED variables
const byte pinR = 4;
const byte pinG = 3;
const byte pinB = 2;
const uint8_t status_LED_colors[][3] = {
  {255, 0, 0},     // ERROR (index 0)
  {255, 132, 0},   // INTERVENTION (index 1)
  {0, 255, 0},     // GOOD (index 2)
  {255, 255, 0},   // BUSY (index 3)
  {0, 255, 255},   // READING (index 4)
  {128, 0, 128}    // WRITING (index 5)
};
enum StatusColor {
  ERROR,
  INTERVENTION,
  GOOD,
  BUSY,
  READING,
  WRITING
};
StatusColor status = INTERVENTION;

// EEPROM variables
EEPROMStorage<int> test(0, 80);
EEPROMStorage<String> text(4, "hello world!");

// Neotimer variables
Neotimer sleep_timer = Neotimer(30000);
Neotimer girouette_read_timer = Neotimer(60000);
bool screen_off = false;

void setup() {
  randomSeed(analogRead(0));
  pinMode(pinR, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(pinB, OUTPUT);
  pinMode(girouette_pin, INPUT);
  LCD.init();
  stepper.setSpeed(10);
  stepper.step(2);
  if (backlight) {
    LCD.backlight();
  }
  // numéro 7 est pour les checkbox
  // slot 0 disponible
  LCD.createChar(1, a_grave_accent);
  LCD.createChar(2, e_acute_accent);
  LCD.createChar(3, e_grave_accent);
  LCD.createChar(4, c_cedilla);
  LCD.createChar(5, degree);
  LCD.createChar(6, girouette_symbol);
  status_LED_color(status);

  Serial.begin(115200);
  Serial.print(test);
  Serial.print(text);
  // while (true) {
  //   Serial.println("Enter rotation angle:");
  //   while (Serial.available() == 0) {
  //   }
  //   int angle = Serial.parseInt();
  //   if (angle > 360 or angle < -360) {
  //     Serial.println("Invalid value!");
  //     delay(500);
  //     continue;
  //   }
  //   Serial.print(String(angle)+" degrees becomes ");
  //   int rotation = map(angle, 0, 360, 1, steps_per_revolution);
  //   Serial.println(String(rotation)+" steps.");
  //   if (angle <= 180) {
  //     stepper.step(-rotation);
  //     delay(1000);
  //     stepper.step(rotation);
  //     delay(1000);
  //   }
  //   else if (angle > 180) {
  //     rotation = steps_per_revolution - rotation;
  //     Serial.println("Rotation angle over 180, spinning "+String(rotation)+" steps in the other direction.");
  //     stepper.step(rotation);
  //     delay(1000);
  //     stepper.step(-rotation);
  //     delay(1000);
  //   }
  // }
}

void clear_data()
{
  data.remove(0);
  data_count = 0;
  return;
}

void eraseEEPROM() {
  reset_progress.setMinValue(0);
  reset_progress.setMaxValue(EEPROM.length() - 1);
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0xFF);
    reset_progress.update(i);
  }
}

void status_LED_color(StatusColor color) {
  analogWrite(pinR, status_LED_colors[color][0]);
  analogWrite(pinG, status_LED_colors[color][1]);
  analogWrite(pinB, status_LED_colors[color][2]);
}

void motor_process() {
  status_LED_color(BUSY);
  if (girouette_direction != previous_direction or girouette_direction == 0) {
    current_step = map(floor(get_opposite_angle(girouette_direction)), 0, 360, 1, steps_per_revolution);
    // if (get_opposite_angle(girouette_direction) - previous_direction > 180 or get_opposite_angle(girouette_direction) - previous_direction < -180) {
    //   current_step = steps_per_revolution - (current_step - previous_step);
    //   if (get_opposite_angle(girouette_direction) - previous_direction < -180) {
    //     current_step = -current_step;
    //   }
    // }
  }
  if (current_step != previous_step) {
    if (current_step - previous_step <= -199) {
      stepper.step(1);
    }
    else if (current_step - previous_step >= 199) {
      stepper.step(-1);
    }
    else {
      stepper.step(current_step - previous_step);
    }
  }
  previous_step = current_step;
  previous_direction = girouette_direction;
  status_LED_color(status);
}

float get_angle_value(int index) {
  if (index >= 0 && index <= 15) {
    return angle_values[index];
  }
  else {
    return -1;
  }
}

float convert_voltage_to_angle(int voltage) {
    int closest_index = 0;
    float min_difference = abs(voltage_values[0] - voltage);
    for (int i = 1; i < 16; ++i) {
        float difference = abs(voltage_values[i] - voltage);
        if (difference < min_difference) {
            min_difference = difference;
            closest_index = i;
        }
    }
    return get_angle_value(closest_index);
}

float get_opposite_angle(float angle) {
  float opposite;
  if (angle == 0) {
    opposite = 180.0;
  }
  else {
    angle = fmod(angle, 360.0);
    opposite = fmod(angle + 180.0, 360.0);
  }
  return opposite;
}

void manage_sleep(char key) {
  if (!key) {
    if (sleep_timer.repeat(1)) {
      LCD.noBacklight();
      LCD.noDisplay();
      screen_off = true;
    }
  }
  else {
    if (screen_off) {
      LCD.backlight();
      LCD.display();
      screen_off = false;
    }
    sleep_timer.repeatReset();
  }
}

void manage_girouette_reading() {
  if (girouette_read_timer.repeat()) {
    girouette_direction = convert_voltage_to_angle(900);
    motor_process();
  }
}

void lock_screen() {
  key_pressed = keypad.getKey();
  LCD.setCursor(0, 0);
  LCD.print("Bienvenue.");
  LCD.setCursor(0, 1);
  LCD.print("Veuillez entrer");
  LCD.setCursor(0, 2);
  LCD.print("votre code:");
  LCD.setCursor(data_count, 3);
  manage_sleep(key_pressed);
  manage_girouette_reading();
  if (key_pressed == KEY_CANCEL) {
    clear_data();
    LCD.clear();
  }
  else if (key_pressed == KEY_ENTER) {
    if (data.compareTo(master) == 0) {
      LCD.clear();
      clear_data();
      status_LED_color(GOOD);
      LCD.print("Code bon!");
      LCD.setCursor(0, 1);
      LCD.print("D\002verouillage...");
      locked = false;
      LCD.noCursor();
      delay(2000);
      status_LED_color(status);
      LCD.clear();
    }
    else {
      LCD.clear();
      clear_data();
      status_LED_color(ERROR);
      LCD.print("Code incorrect!");
      LCD.setCursor(0, 1);
      LCD.print("Retour...");
      LCD.noCursor();
      delay(2000);
      status_LED_color(status);
      LCD.clear();
    }
  }
  else if (key_pressed) {
    if (data_count < 20) {
      data += key_pressed;
      data_count++;
      LCD.print(key_pressed);
    }
    else {
      LCD.clear();
      clear_data();
      status_LED_color(ERROR);
      LCD.print("Code trop long!");
      delay(2000);
      status_LED_color(status);
      LCD.clear();
    }
  }
}

void main_menu() {
  Screen main_menu_screen(20, 4);

  Label main_menu_label("Menu principal:");
  Label settings_label("A pour param\003tres.");
  Label sensors_label("B pour capteurs.");
  Label C_label("C pour actions.");
  Label D_label("D pour ???");

  Button lock_button("Verrouiller");
  Button scroll_button1("");
  Button scroll_button2("");
  Button scroll_button3("");
  Button scroll_button4("");

  ScrollContainer main_menu_scroll_container(&main_menu_screen, main_menu_screen.width(), 2);
  main_menu_scroll_container.add(&settings_label, 0, 0);
  main_menu_scroll_container.add(&scroll_button1, 18, 0);
  main_menu_scroll_container.add(&sensors_label, 0, 1);
  main_menu_scroll_container.add(&scroll_button2, 18, 1);
  main_menu_scroll_container.add(&C_label, 0, 2);
  main_menu_scroll_container.add(&scroll_button3, 18, 2);
  main_menu_scroll_container.add(&D_label, 0, 3);
  main_menu_scroll_container.add(&scroll_button4, 18, 3);

  main_menu_screen.add(&main_menu_label, 0, 0);
  main_menu_screen.add(&main_menu_scroll_container, 0, 1);
  main_menu_screen.add(&lock_button, 0, 3);

  current_screen = "main_menu";

  while (1) {
    main_menu_screen.update();
    if (key_pressed == KEY_A) {
      current_screen = "settings_menu";
      break;
    }
    else if (key_pressed == KEY_B) {
      current_screen = "sensors_menu_1";
      break;
    }
    else if (key_pressed == KEY_C) {
      current_screen = "actions_menu";
      break;
    }
    if (lock_button.pressed()) {
      LCD.clear();
      LCD.print("Verrouillage...");
      locked = true;
      delay(2000);
      LCD.clear();
      break;
    }
    delay(10);
  }
}

void settings_menu() {
  LCD.clear();
  Screen settings_screen(20, 4);

  Label titleLabel("Titre exemple");

  Label testInputLabel("Entr\002e:");
  char previous_input[input_length];
  strcpy(previous_input, input);
  ImprovedInput testInput(input);

  Label colorLabel("Couleur:");
  List colorList(3);
  colorList.addItem("Rouge");
  colorList.addItem("Orange");
  colorList.addItem("Jaune");
  colorList.setSelectedIndex(list_value);

  Label backlightCheckboxLabel("Backlight:");
  Checkbox backlightCheckbox;
  if (backlight) {
    backlightCheckbox.handleInputEvent(0, -1, true, false);
  }

  Label testSpinnerLabel("Arrondi:");
  Spinner testSpinner(arrondi, 1, 30, 1, false);

  ScrollContainer scrollContainer(&settings_screen, settings_screen.width(), 2);
  scrollContainer.add(&testInputLabel, 0, 0);
  scrollContainer.add(&testInput, 7, 0);
  scrollContainer.add(&colorLabel, 0, 1);
  scrollContainer.add(&colorList, 8, 1);
  scrollContainer.add(&backlightCheckboxLabel, 0, 2);
  scrollContainer.add(&backlightCheckbox, 10, 2);
  scrollContainer.add(&testSpinnerLabel, 0, 3);
  scrollContainer.add(&testSpinner, 8, 3);

  Button cancelButton("Annuler");
  Button okButton("Ok");

  settings_screen.add(&titleLabel, 0, 0);
  settings_screen.add(&scrollContainer, 0, 1);
  settings_screen.add(&cancelButton, 0, 3);
  settings_screen.add(&okButton, 16, 3);

  current_screen = "settings_menu";

  while (1) {
    settings_screen.update();
    if (okButton.pressed()) {
      if (backlightCheckbox.checked()) {
        backlight = true;
        LCD.backlight();
      }
      else {
        backlight = false;
        LCD.noBacklight();
      }
      list_value = colorList.selectedIndex();
      arrondi = testSpinner.intValue();
      LCD.clear();
      LCD.print("Modifications");
      LCD.setCursor(0, 1);
      LCD.print("enregistr\002es.");
      delay(2000);
      LCD.clear();
      current_screen = "main_menu";
      break;
    }
    else if (cancelButton.pressed()) {
      strcpy(input, previous_input);
      LCD.clear();
      LCD.print("Modifications");
      LCD.setCursor(0, 1);
      LCD.print("abandonn\002es.");
      delay(2000);
      LCD.clear();
      current_screen = "main_menu";
      break;
    }
    delay(10);
  }
}

void sensors_menu_1() {
  Screen sensors_screen_1(4, 20);

  Label sensors_label("Capteurs:");

  Label info_label_1("Utilisez Suivant et");
  Label info_label_2("Pr\002c. pour naviguer.");
  
  Button back_button("Retour");
  Label page_label("1");
  Button next_button("Suivant");

  sensors_screen_1.add(&sensors_label, 0, 0);
  sensors_screen_1.add(&info_label_1, 0, 1);
  sensors_screen_1.add(&info_label_2, 0, 2);
  sensors_screen_1.add(&back_button, 0, 3);
  sensors_screen_1.add(&page_label, 9, 3);
  sensors_screen_1.add(&next_button, 11, 3);

  current_screen = "sensors_menu_1";

  while (1) {
    sensors_screen_1.update();
    if (back_button.pressed()) {
      current_screen = "main_menu";
      break;
    }
    if (next_button.pressed()) {
      current_screen = "sensors_menu_2";
      break;
    }
  }
}

void sensors_menu_2() {
  char girouette_direction_char[6];
  dtostrf(girouette_direction, 2, 1, girouette_direction_char);

  Screen sensors_screen_2(4, 20);

  Label girouette_label("Girouette:");

  Label direction_label("Dir. vent (\005):");
  Label direction_value(girouette_direction_char);
  Button update_button("M\001j maintenant");

  Button back_button("Pr\002c\002.");
  Label page_label("2");
  Button next_button("Suivant");

  sensors_screen_2.add(&girouette_label, 0, 0);
  sensors_screen_2.add(&direction_label, 0, 1);
  sensors_screen_2.add(&direction_value, 15, 1);
  sensors_screen_2.add(&update_button, 2, 2);
  sensors_screen_2.add(&back_button, 0, 3);
  sensors_screen_2.add(&page_label, 9, 3);
  sensors_screen_2.add(&next_button, 11, 3);

  current_screen = "sensors_menu_2";

  while (1) {
    sensors_screen_2.update();
    if (back_button.pressed()) {
      current_screen = "sensors_menu_1";
      break;
    }
    if (next_button.pressed()) {
      current_screen = "sensors_menu_1";
      break;
    }
    if (update_button.pressed()) {
      dtostrf(girouette_direction, 2, 1, girouette_direction_char);
      direction_value.setText(girouette_direction_char);
    }
  }
}

void actions_menu() {
  Screen actions_screen(4, 20);

  Label actions_label("Actions:");

  Button calibration_button("\x7E");
  Label calibration_label("Calibrer         ");
  Button debug_wind_dir("\x7E");
  Label debug_wind_label("Debug dir. vent  ");
  Button status_cancel_button("\x7E");
  Label status_cancel_label("Eff. alerte LED  ");
  Button reset_button("\x7E");
  Label reset_label("R\002initialiser    ");
  Button back_button("Retour");

  ScrollContainer actions_scroll_container(&actions_screen, actions_screen.width(), 2);
  actions_scroll_container.add(&calibration_label, 0, 0);
  actions_scroll_container.add(&calibration_button, 17, 0);
  actions_scroll_container.add(&debug_wind_label, 0, 1);
  actions_scroll_container.add(&debug_wind_dir, 17, 1);
  actions_scroll_container.add(&status_cancel_label, 0, 2);
  actions_scroll_container.add(&status_cancel_button, 17, 2);
  actions_scroll_container.add(&reset_label, 0, 3);
  actions_scroll_container.add(&reset_button, 17, 3);

  actions_screen.add(&actions_label, 0, 0);
  actions_screen.add(&actions_scroll_container, 0, 1);
  actions_screen.add(&back_button, 0, 3);

  current_screen = "actions_menu";

  while (1) {
    actions_screen.update();
    if (back_button.pressed()) {
      current_screen = "main_menu";
      break;
    }
    else if (calibration_button.pressed()) {
      current_screen = "calibration_menu";
      break;
    }
    else if (debug_wind_dir.pressed()) {
      current_screen = "wind_dir_debug_menu";
      break;
    }
    else if (status_cancel_button.pressed()) {
      status = GOOD;
      status_LED_color(status);
    }
    else if (reset_button.pressed()) {
      status_LED_color(WRITING);
      LCD.clear();
      LCD.print("R\002initialisation,");
      LCD.setCursor(0, 1);
      LCD.print("veuillez patienter.");
      LCD.setCursor(0, 2);
      LCD.print("Progression:");
      eraseEEPROM();
      status_LED_color(status);
      locked = true;
      LCD.setCursor(0, 2);
      LCD.print("Termin\002! Retour...");
      delay(5000);
      LCD.clear();
      break;
    }
  }
}

void calibration_menu() {
  Screen calibration_screen(20, 4);

  Label calibration_label("Calibrer:");
  Label align_label("Aligner avec la \006.");

  Label step_label("Pas:");
  Spinner step_spinner(current_step, 1, steps_per_revolution, 1, true);

  Button finish_button("Terminer");

  calibration_screen.add(&calibration_label, 0, 0);
  calibration_screen.add(&align_label, 0, 1);
  calibration_screen.add(&step_label, 0, 2);
  calibration_screen.add(&step_spinner, 4, 2);
  calibration_screen.add(&finish_button, 0, 3);

  current_screen = "calibration_menu";

  while (1) {
    calibration_screen.update();
    if (finish_button.pressed()) {
      current_step = 1;
      previous_step = 1;
      status = GOOD;
      status_LED_color(status);
      current_screen = "actions_menu";
      break;
    }
    current_step = step_spinner.intValue();
    motor_process();
  }
}

void wind_dir_debug_menu() {
  Screen wind_dir_debug_screen(20, 4);

  Label debug_label("Test direction vent:");
  Label info_label("Emule la valeur.");

  Label direction_label("Angle:");
  List angle_list(16);
  angle_list.addItem("0\005");
  angle_list.addItem("22.5\005");
  angle_list.addItem("45\005");
  angle_list.addItem("67.5\005");
  angle_list.addItem("90\005");
  angle_list.addItem("112.5\005");
  angle_list.addItem("135\005");
  angle_list.addItem("157.5\005");
  angle_list.addItem("180\005");
  angle_list.addItem("202.5\005");
  angle_list.addItem("225\005");
  angle_list.addItem("247.5\005");
  angle_list.addItem("270\005");
  angle_list.addItem("292.5\005");
  angle_list.addItem("315\005");
  angle_list.addItem("337.5\005");

  Button send_button("Envoyer");
  Button cancel_button("Annuler");

  wind_dir_debug_screen.add(&debug_label, 0, 0);
  wind_dir_debug_screen.add(&info_label, 0, 1);
  wind_dir_debug_screen.add(&direction_label, 0, 2);
  wind_dir_debug_screen.add(&angle_list, 6, 2);
  wind_dir_debug_screen.add(&send_button, 0, 3);
  wind_dir_debug_screen.add(&cancel_button, 11, 3);

  current_screen = "wind_dir_debug_menu";

  while (1) {
    wind_dir_debug_screen.update();
    if (cancel_button.pressed()) {
      current_screen = "actions_menu";
      break;
    }
    if (send_button.pressed()) {
      girouette_direction = get_angle_value(angle_list.selectedIndex());
      current_screen = "actions_menu";
      break;
    }
  }
}

void loop() {
  LCD.setCursor(0, 0);

  if (locked) {
      lock_screen();
  }
  else {
    if (current_screen == "main_menu") {
      main_menu();
    }
    else if (current_screen == "settings_menu") {
      settings_menu();
    }
    else if (current_screen == "sensors_menu_1") {
      sensors_menu_1();
    }
    else if (current_screen == "sensors_menu_2") {
      sensors_menu_2();
    }
    else if (current_screen == "actions_menu") {
      actions_menu();
    }
    else if (current_screen == "calibration_menu") {
      calibration_menu();
    }
    else if (current_screen == "wind_dir_debug_menu") {
      wind_dir_debug_menu();
    }
  }
}

void Screen::getInputDeltas(int *x, int *y, bool *selected, bool *cancelled) {
  *x = 0;
  *y = 0;
  key_pressed = keypad.getKey();
  manage_sleep(key_pressed);
  manage_girouette_reading();
  if (key_pressed == KEY_UP) {
    *y = -1;
  }
  else if (key_pressed == KEY_DOWN) {
    *y = 1;
  }
  else if (key_pressed == KEY_LEFT) {
    *x = -1;
  }
  else if (key_pressed == KEY_RIGHT) {
    *x = 1;
  }
  if (key_pressed == KEY_ENTER) {
    *selected = true;
  }
  else {
    *selected = false;
  }
  if (key_pressed == KEY_CANCEL) {
    *cancelled = true;
  }
  else {
    *cancelled = false;
  }
}

void Screen::clear() {
  LCD.clear();
}  

void Screen::createCustomChar(uint8_t slot, uint8_t *data) {
  LCD.createChar(slot, data);
}

void Screen::draw(uint8_t x, uint8_t y, const char *text) {
  LCD.setCursor(x, y);
  LCD.print(text);
}

void Screen::draw(uint8_t x, uint8_t y, uint8_t customChar) {
  LCD.setCursor(x, y);
  LCD.write(customChar);
}

void Screen::setCursorVisible(bool visible) {
  visible ? LCD.cursor() : LCD.noCursor();
}

void Screen::moveCursor(uint8_t x, uint8_t y) {
  LCD.setCursor(x, y);
}

void Screen::setBlink(bool blink) {
  blink ? LCD.blink() : LCD.noBlink();
}