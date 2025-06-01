This program, designed for the ESP32 microcontroller and paired with a SIM800L GSM module, functions as a comprehensive cellular-based location, weather, and notification system. Upon activation—either by pressing the ESP32's BOOT button or sending the character 'y' via Serial—the device collects information about surrounding cell towers, including LAC, CID, MCC, MNC, signal strength, and network operator.

It then uses Google’s Geolocation API to approximate the device’s latitude and longitude, followed by additional API calls to retrieve the corresponding address (via reverse geocoding), local time and timezone, and weather conditions for that location. Connectivity is established through WiFi when available, with automatic fallback to GPRS via SIM800L if needed.

Custom DNS servers are used for both types of connections to ensure stable resolution. Once the data is gathered, it is transmitted via SMS using SIM800L, and optionally via email—though the email feature is currently a placeholder requiring further integration with a library like ESP_Mail_Client. 

Configuration parameters such as WiFi credentials, Google API key, email, phone number, SIM800L pin assignments, and baud rate are defined through constants in the code. The system relies on several libraries including WiFi.h, HTTPClient.h, TinyGsmClient.h, ArduinoJson.h, and SoftwareSerial.h. 


To use the program, the code must be uploaded to the ESP32, the Serial Monitor opened at 115200 baud, and one of the trigger methods used to initiate the data collection process. The project requires an app-specific password, and the Google Weather API endpoint may need adjustment based on availability and access rights.
