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

  // 3. AT+CREG=2
  Serial.println(now() + "Setting CREG mode...");
  sendAT("AT+CREG=2");
  readAT(1000);

  // 4. AT+CREG?
  Serial.println(now() + "Querying registration and cell info...");
  sendAT("AT+CREG?");
  String cregResp = readAT(1500);

  // Parse LAC and CID from +CREG: 2,5,"0495","27B9"
  String lac = "", cid = "";
  int cregIdx = cregResp.indexOf("+CREG:");
  if (cregIdx != -1) {
    int q1 = cregResp.indexOf("\"", cregIdx);
    int q2 = cregResp.indexOf("\"", q1 + 1);
    int q3 = cregResp.indexOf("\"", q2 + 1);
    int q4 = cregResp.indexOf("\"", q3 + 1);
    if (q1 != -1 && q2 != -1 && q3 != -1 && q4 != -1) {
      lac = cregResp.substring(q1 + 1, q2);
      cid = cregResp.substring(q3 + 1, q4);
      long lacDec = strtol(lac.c_str(), NULL, 16);
      long cidDec = strtol(cid.c_str(), NULL, 16);
      g_lac = lacDec;
      g_cid = cidDec;
      Serial.println(now() + "[INFO] LAC: " + lac + " (hex) / " + String(lacDec) + " (dec)");
      Serial.println(now() + "[INFO] CID: " + cid + " (hex) / " + String(cidDec) + " (dec)");
      lac = String(lacDec);
      cid = String(cidDec);
    }
  } else {
    Serial.println(now() + "[ERROR] Failed to parse LAC/CID from SIM800L response.");
  }

  // 5. AT+CSQ
  Serial.println(now() + "Querying signal quality...");
  sendAT("AT+CSQ");
  String csqResp = readAT(1000);

  // Parse signal quality
  int16_t signalQuality = 99;
  int csqIdx = csqResp.indexOf("+CSQ:");
  if (csqIdx != -1) {
    int commaIdx = csqResp.indexOf(",", csqIdx);
    if (commaIdx != -1) {
      String sq = csqResp.substring(csqIdx + 6, commaIdx);
      signalQuality = sq.toInt();
    }
  }
  Serial.println(now() + "[INFO] Signal Quality: " + String(signalQuality));

  // 6. AT+COPS=3,2 (set numeric format for operator)
  Serial.println(now() + "Setting operator format to numeric (AT+COPS=3,2)...");
  sendAT("AT+COPS=3,2");
  String copsSetResp = readAT(1000);
  if (copsSetResp.indexOf("OK") == -1) {
    Serial.println(now() + "[ERROR] Failed to set operator format. Cannot get MCC/MNC.");
    return false;
  }

  // 7. AT+COPS? (get operator numeric code)
  Serial.println(now() + "Querying operator numeric code...");
  sendAT("AT+COPS?");
  String copsResp = readAT(1000);
  String operatorCode = "";
  g_mcc = 0;
  g_mnc = 0;
  int copsNumIdx = copsResp.indexOf("\"");
  int copsNumEnd = copsResp.indexOf("\"", copsNumIdx + 1);
  if (copsNumIdx != -1 && copsNumEnd != -1) {
    String copsNum = copsResp.substring(copsNumIdx + 1, copsNumEnd);
    operatorCode = copsNum;
    if (copsNum.length() >= 5) {
      g_mcc = copsNum.substring(0, 3).toInt();
      g_mnc = copsNum.substring(3).toInt();
      Serial.println(now() + "[INFO] MCC: " + String(g_mcc));
      Serial.println(now() + "[INFO] MNC: " + String(g_mnc));
    }
  }
  Serial.println(now() + "[INFO] Operator Numeric Code: " + operatorCode);

  // 8. AT+COPS=3,0 (set alphanumeric format for operator)
  Serial.println(now() + "Setting operator format to alphanumeric (AT+COPS=3,0)...");
  sendAT("AT+COPS=3,0");
  String copsSetAlphaResp = readAT(1000);
  if (copsSetAlphaResp.indexOf("OK") == -1) {
    Serial.println(now() + "[ERROR] Failed to set operator format. Cannot get operator name.");
    return false;
  }

  // 9. AT+COPS? (get operator name)
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

  // Estimate distance (very rough)
  float frequency = 900.0;
  float distance = pow(10, (27.55 - (20 * log10(frequency)) + abs(signalQuality)) / 20);

  cellInfo = "Operator Name: " + operatorName +
             "\nOperator Numeric Code: " + operatorCode +
             "\nMCC: " + String(g_mcc) +
             "\nMNC: " + String(g_mnc) +
             "\nSignal Quality: " + String(signalQuality) +
             "\nLAC: " + lac +
             "\nCID: " + cid +
             "\nApprox. Distance to Tower: " + String(distance, 1) + " meters";

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
