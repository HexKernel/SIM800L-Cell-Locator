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

bool getCellInfo() {
  Serial.println("\n----------------- SIM800L Section -----------------");
  Serial.println(now() + "Getting cell info...");

  // Helper to send command and print with timestamp and prefix
  auto sendAT = [](const char* cmd) {
    Serial.println(now() + "[CMD] " + cmd);
    sim800Serial.println(cmd);
    delay(100); // Give SIM800L time to process
  };

  // Helper to read and print response with timestamp and prefix
  auto readAT = [&](unsigned long timeout = 1000) {
    String resp = "";
    unsigned long start = millis();
    while (millis() - start < timeout) {
      while (sim800Serial.available()) {
        char c = sim800Serial.read();
        // Only keep printable ASCII and common control chars
        if ((c >= 32 && c <= 126) || c == '\r' || c == '\n') {
          resp += c;
        }
      }
    }
    // Print each line with prefix and timestamp
    int last = 0;
    while (true) {
      int next = resp.indexOf('\n', last);
      String line = (next == -1) ? resp.substring(last) : resp.substring(last, next + 1);
      line.trim();
      if (line.length() > 0) {
        Serial.println(now() + "[RSP] " + line);
      }
      if (next == -1) break;
      last = next + 1;
    }
    return resp;
  };

  // 1. AT
  Serial.println(now() + "Checking SIM800L responsiveness...");
  sendAT("AT");
  String atResp = readAT(1000);
  if (atResp.indexOf("OK") == -1) {
    Serial.println(now() + "[ERROR] SIM800L not responding to AT command.");
    return false;
  }

  // 2. AT+CPIN?
  Serial.println(now() + "Checking SIM card status...");
  sendAT("AT+CPIN?");
  String cpinResp = readAT(1000);
  if (cpinResp.indexOf("READY") == -1) {
    Serial.println(now() + "[ERROR] SIM card not ready or missing.");
    return false;
  }

  // Use AT+CENG? instead of AT+CREG, AT+CSQ, AT+COPS
  Serial.println(now() + "Getting cell info using AT+CENG...");
  sendAT("AT+CENG=3,1"); // Set CENG mode

  String cengResponse = "";
  bool cengSuccess = false;
  for (int i = 0; i < 5; ++i) {
    Serial.println(now() + "Attempt " + String(i + 1) + " to query cell info...");
    sendAT("AT+CENG?");
    cengResponse = readAT(1500);

    // Print raw CENG response
    Serial.println(now() + "Raw AT+CENG? Response:\n" + cengResponse);

    if (cengResponse.indexOf("+CENG:") != -1) {
      cengSuccess = true;
      break;
    } else {
      Serial.println(now() + "[WARN] No CENG info. Retrying...");
      delay(500); // Wait before retrying
    }
  }

  if (!cengSuccess) {
    Serial.println(now() + "[ERROR] Failed to retrieve cell info after multiple attempts.");
    return false;
  }

  // Parse and display CENG data
  int cengIdx = 0;
  while ((cengIdx = cengResponse.indexOf("+CENG:", cengIdx)) != -1) {
    int lineEnd = cengResponse.indexOf("\n", cengIdx);
    if (lineEnd == -1) lineEnd = cengResponse.length();
    String line = cengResponse.substring(cengIdx, lineEnd);
    line.trim();

    int comma1 = line.indexOf(",");
    if (comma1 == -1) {
      cengIdx++;
      continue;
    }
    int index = line.substring(6, comma1).toInt();

    int q1 = line.indexOf("\"", comma1);
    int q2 = line.indexOf("\"", q1 + 1);
    if (q1 == -1 || q2 == -1) {
      cengIdx++;
      continue;
    }
    String data = line.substring(q1 + 1, q2);
    String values[6];
    int k = 0;
    int start = 0;
    for (int j = 0; j < 5; ++j) {
      int comma = data.indexOf(",", start);
      if (comma == -1) break;
      values[k++] = data.substring(start, comma);
      start = comma + 1;
    }
    values[k] = data.substring(start);

    Serial.println(now() + "----------------- Cell " + String(index) + " -----------------");
    if (index == 0) {
      Serial.println(now() + "[INFO] This is the connected cell.");

      // Get operator name for the connected cell
      Serial.println(now() + "Querying operator name...");
      sendAT("AT+COPS?");
      String copsAlphaResp = readAT(1000);
      String operatorName = "";
      int copsAlphaIdx = copsAlphaResp.indexOf("\"");
      int copsAlphaEnd = copsAlphaResp.indexOf("\"", copsAlphaIdx + 1);
      if (copsAlphaIdx != -1 && copsAlphaEnd != -1) {
        operatorName = copsAlphaResp.substring(copsAlphaIdx + 1, copsAlphaEnd);
        Serial.println(now() + "[INFO] Operator Name: " + operatorName);
      } else {
        Serial.println(now() + "[INFO] Operator Name: Not found");
      }
    }

    if (k == 5) {
      if (values[0].length() > 0) Serial.println(now() + "[INFO] MCC: " + values[0]);
      if (values[1].length() > 0) Serial.println(now() + "[INFO] MNC: " + values[1]);

      if (values[2].length() > 0) {
        String lacHex = values[2];
        long lacDec = strtol(lacHex.c_str(), NULL, 16);
        Serial.println(now() + "[INFO] LAC: " + lacHex + " (hex) / " + String(lacDec) + " (dec)");
      }

      if (values[3].length() > 0) {
        String cidHex = values[3];
        long cidDec = strtol(cidHex.c_str(), NULL, 16);
        Serial.println(now() + "[INFO] CID: " + cidHex + " (hex) / " + String(cidDec) + " (dec)");
      }

      if (values[4].length() > 0) Serial.println(now() + "[INFO] RxLev: " + values[4] + " dBm");
      if (values[5].length() > 0) Serial.println(now() + "[INFO] Timing Advance: " + values[5] + " units");
    } else {
      Serial.println(now() + "[WARN] Incomplete data for cell " + String(index));
    }

    cengIdx++;
  }

  //Clear global variables
  g_mcc = 0;
  g_mnc = 0;
  g_lac = 0;
  g_cid = 0;
  cellInfo = "";

  Serial.println(now() + "Cell info query complete.");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  sim800Serial.begin(MODEM_BAUD);
  delay(3000);
  Serial.println("Ready. Type 'y' to get SIM800L cell info.");
}

void loop() {
  static String input = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      input.trim();
      if (input.equalsIgnoreCase("y") || input.equalsIgnoreCase("yes")) {
        getCellInfo();
        Serial.println("Ready. Type 'y' to get SIM800L cell info.");
      }
      input = "";
    } else {
      input += c;
    }
  }
}
