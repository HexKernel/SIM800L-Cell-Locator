#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TinyGsmClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>

// WiFi credentials
const char* WIFI_SSID = "example";
const char* WIFI_PASS = "xxxxxxx";

// Google API Key
const char* GOOGLE_API_KEY = "YOUR_GOOGLE_API_KEY";

// Email settings
const char* EMAIL_TO = "recipient@example.com";
const char* EMAIL_FROM = "your_email@example.com";
const char* EMAIL_PASS = "your_email_password"; // Use app password if Gmail
const char* SMTP_SERVER = "smtp.gmail.com";
const int SMTP_PORT = 465;

// SMS settings
const char* PHONE_NUMBER = "+1234567890";

// SIM800L pins
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 9600

SoftwareSerial sim800Serial(MODEM_RX, MODEM_TX);
TinyGsm modem(sim800Serial);

// Helper variables
String cellInfo = "";
String locationInfo = "";
String addressInfo = "";
String googleMapLink = "";
String allInfo = "";

// Function declarations
bool connectWiFi();
bool connectGPRS();
bool getCellInfo();
bool getLocationFromGoogle();
bool getAddressFromGoogle();
void sendEmail();
void sendSMS();

void setup() {
  Serial.begin(115200);
  delay(1000);

  sim800Serial.begin(MODEM_BAUD);
  delay(3000);

  Serial.println("Starting...");

  // Try WiFi first
  if (connectWiFi()) {
    Serial.println("WiFi connected.");
  } else {
    Serial.println("WiFi not available, trying SIM800L GPRS...");
    if (!connectGPRS()) {
      Serial.println("GPRS connection failed!");
      while (1);
    }
  }

  if (getCellInfo()) {
    Serial.println("Cell info retrieved.");
  } else {
    Serial.println("Failed to get cell info.");
  }

  if (getLocationFromGoogle()) {
    Serial.println("Location info retrieved.");
  } else {
    Serial.println("Failed to get location info.");
  }

  if (getAddressFromGoogle()) {
    Serial.println("Address info retrieved.");
  } else {
    Serial.println("Failed to get address info.");
  }

  // Generate Google Maps link
  googleMapLink = "https://maps.google.com/?q=" + locationInfo;

  // Combine all info
  allInfo = "Cell Info:\n" + cellInfo + "\nLocation (Lat,Lng):\n" + locationInfo +
            "\nAddress:\n" + addressInfo + "\nGoogle Maps:\n" + googleMapLink;

  Serial.println(allInfo);

  sendEmail();
  sendSMS();
}

void loop() {
  // Nothing to do in loop
}

// Connect to WiFi
bool connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

// Connect to GPRS via SIM800L
bool connectGPRS() {
  modem.restart();
  if (!modem.waitForNetwork()) return false;
  if (!modem.gprsConnect("YOUR_APN", "YOUR_USER", "YOUR_PASS")) return false;
  return true;
}

// Get cell info from SIM800L
bool getCellInfo() {
  // Example: AT+CENG or AT+CREG? or AT+CSQ
  String response;
  sim800Serial.println("AT+CREG?");
  delay(500);
  while (sim800Serial.available()) {
    response += char(sim800Serial.read());
  }
  cellInfo = response;
  // Parse and extract more info as needed (LAC, CID, MCC, MNC, etc.)
  // For full info, use AT+CENG or AT+CGED commands and parse response
  return response.length() > 0;
}

// Get location from Google Geolocation API
bool getLocationFromGoogle() {
  // You must extract MCC, MNC, LAC, CID from cellInfo
  // For demo, use dummy values
  int mcc = 222, mnc = 10, lac = 12345, cid = 67890;
  String payload = "{\"cellTowers\":[{\"cellId\":" + String(cid) +
                   ",\"locationAreaCode\":" + String(lac) +
                   ",\"mobileCountryCode\":" + String(mcc) +
                   ",\"mobileNetworkCode\":" + String(mnc) + "}]}";

  HTTPClient http;
  String url = "https://www.googleapis.com/geolocation/v1/geolocate?key=" + String(GOOGLE_API_KEY);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(payload);
  if (httpCode == 200) {
    String resp = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, resp);
    float lat = doc["location"]["lat"];
    float lng = doc["location"]["lng"];
    float accuracy = doc["accuracy"];
    locationInfo = String(lat, 6) + "," + String(lng, 6) + " (Accuracy: " + String(accuracy) + "m)";
    http.end();
    return true;
  }
  http.end();
  return false;
}

// Get address from Google Reverse Geocoding API
bool getAddressFromGoogle() {
  // Extract lat/lng from locationInfo
  float lat = 0, lng = 0;
  sscanf(locationInfo.c_str(), "%f,%f", &lat, &lng);
  HTTPClient http;
  String url = "https://maps.googleapis.com/maps/api/geocode/json?latlng=" +
               String(lat, 6) + "," + String(lng, 6) + "&key=" + String(GOOGLE_API_KEY);
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String resp = http.getString();
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, resp);
    addressInfo = doc["results"][0]["formatted_address"].as<String>();
    http.end();
    return true;
  }
  http.end();
  return false;
}

// Send email (use ESP_Mail_Client or similar library)
void sendEmail() {
  // Placeholder: Implement using ESP_Mail_Client or SMTP library
  Serial.println("Sending email (not implemented in this scaffold)...");
}

// Send SMS via SIM800L
void sendSMS() {
  sim800Serial.println("AT+CMGF=1"); // Set SMS to text mode
  delay(1000);
  sim800Serial.print("AT+CMGS=\"");
  sim800Serial.print(PHONE_NUMBER);
  sim800Serial.println("\"");
  delay(1000);
  sim800Serial.print(allInfo);
  delay(500);
  sim800Serial.write(26); // Ctrl+Z to send
  delay(5000);
  Serial.println("SMS sent.");
}
