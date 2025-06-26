#include <painlessMesh.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555

#define STATION_SSID     "EREN"
#define STATION_PASSWORD "eren1456"

#define MQTT_SERVER "test.mosquitto.org" // MQTT broker adresi
#define MQTT_PORT 1883

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

painlessMesh mesh;

// Her 5 saniyede bir kendini root olarak duyurur
unsigned long lastBroadcast = 0;

// MQTT ye bağlanması için kod yazıldı
void connectToMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Connecting...");
    if (mqttClient.connect("rootNodeClient")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

// Alt nodelardan gelen mesajlar
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("[ROOT] 🔔 Received from %u: %s\n", from, msg.c_str());

  // Gelen mesaj JSON mu?
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, msg);

  if (!error) {
    const char* deviceId = doc["device_id"];
    long uptime = doc["uptime"];
    int rssi = doc["rssi"];
    Serial.printf("[ROOT] 📦 Device: %s | Uptime: %ld s | RSSI: %d dBm\n", deviceId, uptime, rssi);
    String topic = "robot/metrics/";
    topic += deviceId;

    String mqttMsg;
    serializeJson(doc, mqttMsg);

    if (!mqttClient.connected()) {
      connectToMQTT();
    }

    if (mqttClient.publish(topic.c_str(), mqttMsg.c_str())) {
      Serial.printf("[MQTT] Published to %s >> %s\n", topic.c_str(), mqttMsg.c_str());
    } else {
      Serial.println("[MQTT] Publish failed");
    }


  } else {
    Serial.println("[ROOT] ⚠️ JSON parsing error");
  }
}

// WiFi bağlantısını kontrol etmesi için yazıldı
// WiFi olmadan MQTT'ye bağlanmaya çalışmaması gerekiyor.
void waitForWiFi(unsigned long timeout = 15000) {
  Serial.print("[WiFi] Connecting");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
  } else {
    Serial.println("\n[WiFi] Connection timed out!");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(">>> 🟢 Starting ROOT node...");

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 6);
  mesh.setRoot(true);
  mesh.setContainsRoot(true);
  mesh.onReceive(&receivedCallback);

  // WiFi bağlantısı
  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  waitForWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println(">>> 🌐 Mesh + Station init complete.");
}

unsigned long lastMQTTRetry = 0;
const unsigned long mqttRetryInterval = 2000; // 2 saniye

void loop() {
  mesh.update();

  // Root broadcast her 5 saniye
  if (millis() - lastBroadcast > 5000) {
    String rootMsg = "ROOT:" + String(mesh.getNodeId());
    mesh.sendBroadcast(rootMsg);
    Serial.printf("[ROOT] 📡 Broadcasting ROOT ID: %s\n", rootMsg.c_str());
    lastBroadcast = millis();
  }

  // WiFi durum kontrolü her 10 saniye
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {
    lastWiFiCheck = millis();
    Serial.print("[WiFi] Status: ");
    Serial.println(WiFi.status());
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
  }

  // WiFi bağlı değilse her saniye nokta yazdır
  static unsigned long lastDotPrint = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastDotPrint > 1000) {
      Serial.print(".");
      lastDotPrint = millis();
    }
  }

  // MQTT bağlantısını kontrol et, sadece WiFi bağlıysa
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      if (millis() - lastMQTTRetry > mqttRetryInterval) {
        Serial.println("[MQTT] Attempting MQTT connection...");
        if (mqttClient.connect("rootNodeClient")) {
          Serial.println("[MQTT] Connected");
        } else {
          Serial.printf("[MQTT] Failed rc=%d, will retry\n", mqttClient.state());
        }
        lastMQTTRetry = millis();
      }
    } else {
      mqttClient.loop();
    }
  }
}


