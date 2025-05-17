// Get cell info from SIM800L
/**
 * @brief Queries the SIM800L GSM module for current cell information.
 *
 * This function is part of the `main3.cpp` file, which has been specifically created 
 * to **isolate and test SIM800L AT command interactions independently** of the main project logic. 
 * The purpose of this modular approach is to **facilitate debugging, ensure proper configuration**, 
 * and verify communication with the SIM800L before integrating the working code into `main.cpp`.
 *
 * It communicates with the SIM800L module via AT commands to retrieve:
 *   - Module responsiveness (AT)
 *   - SIM card status (AT+CPIN?)
 *   - Network registration and cell info (AT+CREG=2, AT+CREG?)
 *   - Signal quality (AT+CSQ)
 *   - Operator name and network codes (AT+COPS?)
 *
 * It parses the responses to extract:
 *   - Location Area Code (LAC)
 *   - Cell ID (CID)
 *   - Signal quality (RSSI)
 *   - Operator name
 *   - Mobile Country Code (MCC) and Mobile Network Code (MNC)
 *   - An approximate distance to the cell tower (rough estimate only)
 *
 * The extracted information is stored in global variables (g_lac, g_cid, g_mcc, g_mnc)
 * and a summary string (cellInfo).
 *
 * @return true if all required information was successfully retrieved and parsed, false otherwise.
 */
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <math.h>

// SIM800L pins and baud
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 9600

SoftwareSerial sim800Serial(MODEM_RX, MODEM_TX);

// Globals for parsed cell info
int g_mcc = 0, g_mnc = 0, g_lac = 0, g_cid = 0;
String cellInfo = "";

// Timestamp helper
String now() {
  unsigned long ms = millis();
  unsigned long s = ms / 1000;
  unsigned long m = s / 60;
  unsigned long h = m / 60;
  char buf[16];
  sprintf(buf, "[%02lu:%02lu:%02lu] ", h % 24, m % 60, s % 60);
  return String(buf);
}
