#include <SoftwareSerial.h>

#define numRows 9 //接收信号
#define numCols 9 //发出信号
#define sensorPoints numRows*numCols

int rows[] = {A3, A4, A5, A6, A7, A8, A9, A10, A11};
int cols[] = {2, 3, 4, 5, 6, 7, 8, 9, 10};

int incomingValues[sensorPoints] = {}; //收集到的数据点集

SoftwareSerial mySerial(11, 12); // RX, TX

void setup() {
  // set all rows and columns to INPUT (high impedance):
  for (int i = 0; i < numRows; i++) {
    pinMode(rows[i], INPUT_PULLUP);
  }
  for (int i = 0; i < numCols; i++) {
    pinMode(cols[i], INPUT);
  }
  mySerial.begin(9600);
  Serial.begin(9600);
}

void loop() {
  // 等待ESP32的触发信号
  if (mySerial.available()) {
    String trigger = mySerial.readStringUntil('\n');
    if (trigger.compareTo("TRIGGER")) {
      for (int colCount = 0; colCount < numCols; colCount++) {
        pinMode(cols[colCount], OUTPUT); // set as OUTPUT
        digitalWrite(cols[colCount], LOW); // set LOW

        for (int rowCount = 0; rowCount < numRows; rowCount++) {
          int press = analogRead(rows[rowCount]);
          incomingValues[colCount * numRows + rowCount] = press; // press受压会变小，~阻值
        }// end rowCount

        pinMode(cols[colCount], INPUT); // set back to INPUT!

      }// end colCount

      // Print the incoming values of the grid:
      for (int i = 0; i < sensorPoints; i++) {
        mySerial.print(incomingValues[i]);
        if (i < sensorPoints - 1) {
          mySerial.print("\t");
        }
        delay(5); // 延迟以确保数据传输稳定
      }
      mySerial.println();
    }
  }
}
