class ProgressBar {
  public:
    ProgressBar(LiquidCrystal_I2C& lcd, int length, int row, int col) : 
      lcd(lcd), length(length), row(row), col(col), minValue(0), maxValue(100), curValue(0) {}

    void setMinValue(int minValue) {
      this->minValue = minValue;
    }

    void setMaxValue(int maxValue) {
      this->maxValue = maxValue;
    }

    void update(int curValue) {
      this->curValue = curValue;
      draw();
    }

  private:
    LiquidCrystal_I2C& lcd;
    int length;
    int row;
    int col;
    int minValue;
    int maxValue;
    int curValue;

    void draw() {
      lcd.setCursor(col, row);
      lcd.print("[");
      int completedSegments = map(curValue, minValue, maxValue, 0, length);
      for (int i = 0; i < length; i++) {
        if (i < completedSegments) {
          lcd.print("=");
        } else {
          lcd.print(".");
        }
      }
      lcd.print("] ");

      int percentage = map(curValue, minValue, maxValue, 0, 100);
      lcd.print(percentage);
      lcd.print("%");
    }
};