#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>

// Wi-Fi credentials
const char* ssid = "xxxxx";
const char* password = "xxxxxx";

// MQTT Broker
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* mqttClientID = "esp32-client3";
const char* mqttTopic = "xxxxxxx";

// MAC addresses of ESP1 and ESP2
uint8_t receiverMAC1[] = {0xb0, 0xa7, 0x32, 0x2b, 0x23, 0x84}; // ESP1
uint8_t receiverMAC2[] = {0xb0, 0xa7, 0x32, 0x2a, 0x8e, 0x70}; // ESP2

// PIN for authentication
const String pin = "xxxxxx"; // Secret PIN for verification

// Wi-Fi and MQTT clients
WiFiClient espClient; 
PubSubClient client(espClient);

// Message structure
typedef struct Message {
  char text[32];
  char pin[5];
} Message;

Message outgoingMessage;
Message incomingMessage;

String esp3Message = "Halo 3"; // Message from ESP3
String esp1Message = "";       // Received from ESP1
String esp2Message = "";       // Received from ESP2

// Fungsi untuk mencetak alamat MAC
String macToString(const uint8_t *macAddr) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           macAddr[0], macAddr[1], macAddr[2],
           macAddr[3], macAddr[4], macAddr[5]);
  return String(macStr);
}

// Callback for receiving data via ESP-NOW
void onDataReceive(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));

  // Verifikasi PIN
  if (String(incomingMessage.pin) != pin) {
    Serial.println("PIN salah! Pesan ditolak.");
    return;
  }

  // Verifikasi MAC address pengirim
  if (memcmp(info->src_addr, receiverMAC1, 6) == 0) {
    esp1Message = String(incomingMessage.text);
    Serial.printf("Pesan valid, pengirim adalah ESP1 [%s]\n", macToString(info->src_addr).c_str());
    Serial.printf("Pesan diterima dari ESP1: %s\n", incomingMessage.text);
  } else if (memcmp(info->src_addr, receiverMAC2, 6) == 0) {
    esp2Message = String(incomingMessage.text);
    Serial.printf("Pesan valid, pengirim adalah ESP2 [%s]\n", macToString(info->src_addr).c_str());
    Serial.printf("Pesan diterima dari ESP2: %s\n", incomingMessage.text);
  } else {
    Serial.println("MAC address tidak dikenal. Pesan ditolak.");
  }
}

// Connect to Wi-Fi
void connectToWiFi() {
  Serial.println("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");
}

// Reconnect to MQTT broker
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke broker MQTT...");
    if (client.connect(mqttClientID)) {
      Serial.println("Berhasil terhubung ke MQTT.");
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

// Send data to MQTT
void sendMQTTMessage() {
  String mqttMessage = "ESP3 = " + esp1Message + ", " + esp2Message + ", " + esp3Message;
  client.publish(mqttTopic, mqttMessage.c_str());
  Serial.printf("Mengirim pesan ke MQTT: %s\n", mqttMessage.c_str());
}

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  connectToWiFi();

  // Initialize MQTT
  client.setServer(mqttServer, mqttPort);
  reconnectMQTT();

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW gagal diinisialisasi.");
    return;
  }
  esp_now_register_recv_cb(onDataReceive);

  // Add peers
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));

  memcpy(peerInfo.peer_addr, receiverMAC1, 6);
  esp_now_add_peer(&peerInfo);

  memcpy(peerInfo.peer_addr, receiverMAC2, 6);
  esp_now_add_peer(&peerInfo);
}

void loop() {
  // Prepare message for ESP1 and ESP2
  strcpy(outgoingMessage.text, esp3Message.c_str());
  strcpy(outgoingMessage.pin, pin.c_str());

  esp_now_send(receiverMAC1, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
  Serial.printf("Mengirim pesan ke ESP1: %s\n", esp3Message.c_str());

  esp_now_send(receiverMAC2, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
  Serial.printf("Mengirim pesan ke ESP2: %s\n", esp3Message.c_str());

  // Send data to MQTT every 5 seconds
  static unsigned long lastTime = 0;
  unsigned long now = millis();
  if (now - lastTime > 5000) {
    lastTime = now;
    if (!client.connected()) {
      reconnectMQTT();
    }
    sendMQTTMessage();
  }

  delay(5000);
}
