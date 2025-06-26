#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Ping.h>

// Wi-Fi & MQTT Bilgileri
const char* ssid = "EREN";
const char* password = "eren1456";
const char* mqtt_server = "test.mosquitto.org";  // MQTT broker IP
const char* mqtt_topic = "robot/metrics/robot_5";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastSend = 0;
unsigned long startTime;
String device_id;

void setup_wifi() {
  WiFi.begin(ssid, password);
  startTime = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}


void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(device_id.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());  // Hata kodu
      Serial.println(" try again in 3 seconds");
      delay(3000);
    }
  }
}


void sendMetrics() {
  StaticJsonDocument<512> doc;
  unsigned long now = millis();
  doc["device_id"] = device_id;
  doc["timestamp"] = now;
  doc["uptime"] = (now - startTime) / 1000;
  doc["rssi"] = WiFi.RSSI();
  doc["bandwidth"] = "20MHz";  // sabit

  int packetSent = 5;
  int packetReceived = 0;

  for (int i = 0; i < packetSent; i++) {
    if (Ping.ping("8.8.8.8", 1)) {
      packetReceived++;
    }
    delay(100);
  }

  if (packetReceived > 0) {
    doc["latency"] = Ping.averageTime();
  } else {
    doc["latency"] = -1;
  }

  doc["packet_loss"] = packetSent - packetReceived;

  String output;
  serializeJson(doc, output);
  Serial.println(output);
  client.publish(mqtt_topic, output.c_str());
}


void setup() {
  Serial.begin(115200);             // <-- BURADA OLMALI
  delay(1000);                      // USB için zaman tanı
  Serial.println("Starting...");

  device_id = "robot_2";
  setup_wifi();
  client.setServer(mqtt_server, 1883);
}


void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  if (millis() - lastSend > 1000) {
    lastSend = millis();
    sendMetrics();
  }
}
