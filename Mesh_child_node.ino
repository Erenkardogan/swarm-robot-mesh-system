#include "painlessMesh.h"
#include <ArduinoJson.h>

#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555
#define MESH_CHANNEL    6  // Root ile aynı kanal

Scheduler userScheduler;
painlessMesh mesh;
uint32_t rootId = 0;

String device_id = "robot_1";

// ROOT mesajı alındığında
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("[NODE] 🔔 Message from %u: %s\n", from, msg.c_str());

  if (msg.startsWith("ROOT:")) {
    uint32_t receivedRootId = (uint32_t)strtoul(msg.substring(5).c_str(), NULL, 10);

    if (receivedRootId != rootId && receivedRootId != 0 && receivedRootId != 2147483647UL) {
      rootId = receivedRootId;
      Serial.printf("[NODE] ✅ Learned root ID: %u\n", rootId);
    }
  } else {
    Serial.println("[NODE] 🔹 Non-root message received.");
  }
}

// Yeni bağlantı sağlandığında
void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[MESH] 🔗 Connected to: %u\n", nodeId);
}

// Root’a veri gönderme
void sendMessageToRoot() {
  if (rootId == 0 || rootId == 2147483647UL) {
    Serial.println("[SEND] ⚠️ Invalid rootId, not sending.");
    return;
  }

  StaticJsonDocument<256> doc;
  doc["device_id"] = device_id;
  doc["uptime"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();

  String msg;
  serializeJson(doc, msg);
  mesh.sendSingle(rootId, msg);
  Serial.printf("[SEND] 📤 Sent to %u >> %s\n", rootId, msg.c_str());
}

Task sendTask(TASK_SECOND * 5, TASK_FOREVER, &sendMessageToRoot);

void setup() {
  Serial.begin(115200);
  Serial.println(">>> Starting ALT NODE...");

  // Mesh başlatma
  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL); // Kanal 6 sabitlendi
  mesh.setContainsRoot(true);  // Root’tan gelen yayınların iletilmesini sağlar
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);

  userScheduler.addTask(sendTask);
  sendTask.enable();
}

void loop() {
  mesh.update();
  userScheduler.execute();
}
