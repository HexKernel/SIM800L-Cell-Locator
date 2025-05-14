#include <Arduino.h>
#include <SoftwareSerial.h>

#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 9600
#define PC_BAUD 115200  // Default baud rate for laptop serial

SoftwareSerial sim800Serial(MODEM_RX, MODEM_TX);

void setup() {
  Serial.begin(PC_BAUD); // Set to 115200 for laptop
  sim800Serial.begin(MODEM_BAUD); // 9600 for SIM800L
  delay(3000);
  Serial.println("SIM800L Serial Bridge Ready.");
}
