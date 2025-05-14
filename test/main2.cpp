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

void loop() {
  // Pass data from Serial (laptop) to SIM800L
  while (Serial.available()) {
    char c = Serial.read();
    sim800Serial.write(c);
  }

  // Pass filtered data from SIM800L to Serial (laptop)
  while (sim800Serial.available()) {
    char c = sim800Serial.read();
    // Only forward printable ASCII and common control chars
    if ((c >= 32 && c <= 126) || c == '\r' || c == '\n') {
      Serial.write(c);
    }
    // Optionally, print a warning for filtered garbage
    // else Serial.print("[?]");
  }
}
