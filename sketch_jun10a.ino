#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
#include <Update.h>

// SIMCOM control pins
#define SIM_PWR 4
#define SIM_RX  27
#define SIM_TX  26

#define TINY_GSM_USE_GPRS true

// Set your SIM APN
const char APN[] = "airtelgprs.com";

// CDN host and binary file path
const char FW_HOST[] = "cdn.jsdelivr.net";
const int  FW_PORT   = 80;
const char FW_PATH[] = "/gh/Aditya-agarwal18/imgproc@e23a1f0645002a55eaf2f64445e2ed6eef1ea235/sketch_jun10a.ino.bin";

TinyGsm modem(Serial1);
TinyGsmClient client(modem);

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("Starting GSM OTA updater...");

  pinMode(SIM_PWR, OUTPUT);
  digitalWrite(SIM_PWR, LOW); delay(100);
  digitalWrite(SIM_PWR, HIGH); delay(1000);
  digitalWrite(SIM_PWR, LOW); delay(3000);

  Serial1.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
  while (!modem.init()) {
    Serial.println("Modem initialization failed. Retrying...");
    delay(2000);
  }

  Serial.println("Waiting for network registration...");
  while (!modem.waitForNetwork(60000L)) {
    Serial.println("Still waiting...");
  }

  Serial.print("Connecting GPRS with APN ");
  Serial.println(APN);
  while (!modem.gprsConnect(APN)) {
    Serial.println("GPRS connection failed. Retrying...");
    delay(2000);
  }

  Serial.print("GPRS connected. IP: ");
  Serial.println(modem.getLocalIP());

  performOTA();
}

void loop() {
  // No actions needed here
}

void performOTA() {
  Serial.printf("Connecting to %s:%d\n", FW_HOST, FW_PORT);
  if (!client.connect(FW_HOST, FW_PORT)) {
    Serial.println("Connection to server failed");
    return;
  }

  String getRequest = String("GET ") + FW_PATH + " HTTP/1.0\r\n" +
                      "Host: " + FW_HOST + "\r\n" +
                      "Connection: close\r\n\r\n";
  client.print(getRequest);
  Serial.println("GET request sent:");
  Serial.println(getRequest);

  size_t contentLength = 0;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("Content-Length: ")) {
      contentLength = line.substring(16).toInt();
      Serial.printf("Found Content-Length: %u bytes\n", contentLength);
    }
    if (line == "\r") {
      Serial.println("End of headers");
      break;
    }
  }

  if (contentLength == 0) {
    Serial.println("No Content-Length found. Aborting OTA.");
    client.stop();
    return;
  }

  if (!Update.begin(contentLength)) {
    Serial.printf("OTA begin failed. Error #%u\n", Update.getError());
    client.stop();
    return;
  }

  size_t written = Update.writeStream(client);
  Serial.printf("Written %u bytes\n", written);
  if (written != contentLength) {
    Serial.printf("Warning: only wrote %u of %u bytes\n", written, contentLength);
  }

  if (Update.end(true)) {
    Serial.println("OTA successful! Restarting...");
    delay(100);
    ESP.restart();
  } else {
    Serial.printf("OTA failed. Error #%u\n", Update.getError());
  }

  client.stop();
}
