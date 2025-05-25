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
#include <math.h>
#include <map>

// SIM800L pins and baud
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 9600

HardwareSerial sim800Serial(2); // Use UART2

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

bool isCengDataComplete(const String& cengResponse) {
  int cengIdx = 0;
  while ((cengIdx = cengResponse.indexOf("+CENG:", cengIdx)) != -1) {
    int lineEnd = cengResponse.indexOf("\n", cengIdx);
    if (lineEnd == -1) lineEnd = cengResponse.length();
    String line = cengResponse.substring(cengIdx, lineEnd);
    line.trim();

    // Only process lines with a quoted data section (cell info lines)
    int comma1 = line.indexOf(",");
    int q1 = line.indexOf("\"", comma1);
    int q2 = line.indexOf("\"", q1 + 1);
    if (q1 == -1 || q2 == -1) {
      cengIdx = lineEnd; // skip header or malformed lines
      continue;
    }
    String data = line.substring(q1 + 1, q2);
    String values[6];
    int k = 0;
    int start = 0;
    for (int j = 0; j < 5; ++j) {
      int comma = data.indexOf(",", start);
      if (comma == -1) {
        values[k++] = data.substring(start);
        break;
      }
      values[k++] = data.substring(start, comma);
      start = comma + 1;
    }
    while (k < 6) values[k++] = "";

    // Check for completeness: no column should be empty, "0000", or "ffff"
    for (int i = 0; i < 4; ++i) { // Only check MCC, MNC, LAC, CID
      String v = values[i];
      v.trim();
      v.toLowerCase();
      if (v.length() == 0 || v == "0000" || v == "ffff") {
        return false;
      }
    }
    cengIdx = lineEnd;
  }
  return true;
}

bool getCellInfo() {
  Serial.println("\n----------------- SIM800L Section -----------------");
  Serial.println(now() + "Getting cell info...");

  // Helper to send command and print with timestamp and prefix
  auto sendAT = [](const char* cmd) {
    Serial.println(now() + "[CMD] " + cmd);
    sim800Serial.println(cmd);
    delay(200); // Give SIM800L more time to process
  };

  // Helper to read and print response with timestamp and prefix
  auto readAT = [&](unsigned long timeout = 3000) { // Increased timeout
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
      delay(10); // Allow other tasks to run
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
  delay(500);

  String cengResponse = "";
  bool cengSuccess = false;
  int successfulRound = -1;
  for (int i = 0; i < 5; ++i) {
    Serial.println(now() + "[INFO] Attempt " + String(i + 1) + " checking completeness of AT+CENG?...");
    sendAT("AT+CENG?");
    cengResponse = readAT(3000);

    if (cengResponse.indexOf("+CENG:") != -1 && isCengDataComplete(cengResponse)) {
      cengSuccess = true;
      successfulRound = i + 1;
      Serial.println(now() + "[INFO] Round " + String(successfulRound) + " checking was successful.");
      break;
    } else {
      Serial.println(now() + "[WARN] CENG data incomplete, retrying...");
      delay(500);
    }
  }

  if (!cengSuccess) {
    Serial.println(now() + "[ERROR] Failed to retrieve complete cell info after multiple attempts.");
    return false;
  }

  // Show parsing log and loading animation
  Serial.println(now() + "[INFO] Parsing CENG data...");
  for (int i = 0; i < 3; ++i) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();

  // Parse and display CENG data
  // First, collect all cell lines into a map by index
  std::map<int, String> cellLines;
  int maxIndex = -1;
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
    cellLines[index] = line;
    if (index > maxIndex) maxIndex = index;
    cengIdx = lineEnd;
  }

  // Print info for each cell index, even if missing
  for (int idx = 0; idx <= maxIndex; ++idx) {
    Serial.println(now() + "----------------- Cell " + String(idx) + " -----------------");
    if (cellLines.count(idx) == 0) {
      Serial.println(now() + "[WARN] Incomplete data for cell " + String(idx));
      continue;
    }
    String line = cellLines[idx];

    int comma1 = line.indexOf(",");
    int q1 = line.indexOf("\"", comma1);
    int q2 = line.indexOf("\"", q1 + 1);
    if (q1 == -1 || q2 == -1) {
      Serial.println(now() + "[WARN] Incomplete data for cell " + String(idx));
      continue;
    }
    String data = line.substring(q1 + 1, q2);
    String values[6];
    int k = 0;
    int start = 0;
    for (int j = 0; j < 5; ++j) {
      int comma = data.indexOf(",", start);
      if (comma == -1) {
        values[k++] = data.substring(start);
        break;
      }
      values[k++] = data.substring(start, comma);
      start = comma + 1;
    }
    while (k < 6) values[k++] = "";

    if (idx == 0) {
      Serial.println(now() + "[INFO] This is the connected cell.");
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

    if (values[0].length() > 0 && values[1].length() > 0 && values[2].length() > 0 && values[3].length() > 0) {
      Serial.println(now() + "[INFO] MCC: " + values[0]);
      Serial.println(now() + "[INFO] MNC: " + values[1]);
      String lacHex = values[2];
      long lacDec = strtol(lacHex.c_str(), NULL, 16);
      Serial.println(now() + "[INFO] LAC: " + lacHex + " (hex) / " + String(lacDec) + " (dec)");
      String cidHex = values[3];
      long cidDec = strtol(cidHex.c_str(), NULL, 16);
      Serial.println(now() + "[INFO] CID: " + cidHex + " (hex) / " + String(cidDec) + " (dec)");
      if (values[4].length() > 0) {
        int rxLev = values[4].toInt();
        int rxDbm = -113 + (2 * rxLev);
        Serial.println(now() + "[INFO] RxLev: " + values[4] + " (unit) / " + String(rxDbm) + " (dBm)");
      }
      if (values[5].length() > 0) Serial.println(now() + "[INFO] Timing Advance: " + values[5] + " units");
    } else {
      Serial.println(now() + "[WARN] Incomplete data for cell " + String(idx));
    }
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

  // Initialize HardwareSerial for SIM800L
  sim800Serial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
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
