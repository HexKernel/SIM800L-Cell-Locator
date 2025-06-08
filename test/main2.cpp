/**
 * @file main2.cpp
 * @brief Serial bridge between a PC and SIM800L GSM module using Arduino.
 *
 * This program sets up a serial bridge between a PC (via the main Serial port)
 * and a SIM800L GSM module (via SoftwareSerial). It allows the user to send AT
 * commands from the PC to the SIM800L and receive responses, filtering out
 * non-printable characters from the SIM800L output.
 *
 * - MODEM_RX: GPIO pin connected to SIM800L TX (default: 16)
 * - MODEM_TX: GPIO pin connected to SIM800L RX (default: 17)
 * - MODEM_BAUD: Baud rate for SIM800L communication (default: 9600)
 * - PC_BAUD: Baud rate for PC serial communication (default: 115200)
 *
 * Functionality:
 * - Forwards all data from the PC Serial to the SIM800L.
 * - Forwards only printable ASCII characters and common control characters
 *   (CR, LF) from the SIM800L to the PC Serial, filtering out garbage data.
 *
 * Usage:
 * - Connect the PC to the Arduino via USB.
 * - Connect the SIM800L module to the specified RX/TX pins.
 * - Open a serial monitor at 115200 baud to interact with the SIM800L.
 */
#include <Arduino.h>

#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 9600
#define PC_BAUD 115200  

HardwareSerial sim800Serial(2); 

void setup() {
  Serial.begin(PC_BAUD);
  sim800Serial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX); 
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
  }
}
