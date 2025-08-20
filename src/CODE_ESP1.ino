#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>

// Wi-Fi credentials
const char* ssid = "xxxxxxx";
const char* password = "xxxxxx";

// MQTT Broker
const char* mqttServer = "broker.hivemq.com"; //alamat broker MQTT yang digunakan untuk mengelola komunikasi antar perangkat melalui protokol MQTT.
const int mqttPort = 1883; // port yang digunakan untuk koneksi ke broker MQTT.
const char* mqttClientID = "esp32-client1"; // ID unik yang digunakan oleh perangkat ESP32 untuk mengidentifikasi dirinya ke broker MQTT.
const char* mqttTopic = "xxxxx"; // nama topik MQTT yang digunakan untuk mengirim dan menerima pesan melalui broker MQTT.

// MAC addresses of ESP2 and ESP3
uint8_t receiverMAC2[] = {0xb0, 0xa7, 0x32, 0x2a, 0x8e, 0x70}; // ESP2
uint8_t receiverMAC3[] = {0x40, 0x91, 0x51, 0xfc, 0x07, 0xa8}; // ESP3

// PIN for authentication
const String pin = "xxxxx"; // Secret PIN for verification

// Wi-Fi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient); 

// Message structure
typedef struct Message {
  char text[32];
  char pin[5];
} Message;

Message outgoingMessage; //deklarasi variable untuk pesan yg keluar
Message incomingMessage;

String esp1Message = "Halo 1"; // Message from ESP1
String esp2Message = "";       // Received from ESP2
String esp3Message = "";       // Received from ESP3

// Fungsi untuk mencetak alamat MAC
String macToString(const uint8_t *macAddr) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           macAddr[0], macAddr[1], macAddr[2],
           macAddr[3], macAddr[4], macAddr[5]);
  return String(macStr);
}

// callback yang dipanggil secara otomatis saat data diterima via ESP-NOW
void onDataReceive(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));

  // Verifikasi PIN
  if (String(incomingMessage.pin) != pin) {
    Serial.println("PIN salah! Pesan ditolak.");
    return;
  }

  // Verifikasi MAC address pengirim
  if (memcmp(info->src_addr, receiverMAC2, 6) == 0) {
    esp2Message = String(incomingMessage.text);
    Serial.printf("Pesan valid, pengirim adalah ESP2 [%s]\n", macToString(info->src_addr).c_str());
    Serial.printf("Pesan diterima dari ESP2: %s\n", incomingMessage.text);
  } else if (memcmp(info->src_addr, receiverMAC3, 6) == 0) {
    esp3Message = String(incomingMessage.text);
    Serial.printf("Pesan valid, pengirim adalah ESP3 [%s]\n", macToString(info->src_addr).c_str());
    Serial.printf("Pesan diterima dari ESP3: %s\n", incomingMessage.text);
  } else {
    Serial.println("Alamat MAC address tidak valid. Pesan diblokir.");
  }
}

//---------------------------------------------------------------------------------------------------------

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
  String mqttMessage = "ESP1 = " + esp1Message + ", " + esp2Message + ", " + esp3Message;
  client.publish(mqttTopic, mqttMessage.c_str());
  Serial.printf("Mengirim pesan ke MQTT: %s\n", mqttMessage.c_str());
}

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  connectToWiFi();

  // Inisialisasi MQTT
  client.setServer(mqttServer, mqttPort);
  reconnectMQTT();

  // Inisialisasi ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW gagal diinisialisasi.");
    return;
  }
  esp_now_register_recv_cb(onDataReceive);

  // Add peers
  esp_now_peer_info_t peerInfo; // menyimpan informasi tentang perangkat yang akan menjadi peer (misalnya, alamat MAC).
  memset(&peerInfo, 0, sizeof(peerInfo));

  memcpy(peerInfo.peer_addr, receiverMAC2, 6); //peer.info : variabel dalam struktur peerInfo yang menyimpan alamat MAC dari peer.
  esp_now_add_peer(&peerInfo);

  memcpy(peerInfo.peer_addr, receiverMAC3, 6);
  esp_now_add_peer(&peerInfo);
}

void loop() {
  // menyiapkan pesan untuk ESP2 and ESP3
  strcpy(outgoingMessage.text, esp1Message.c_str());
  strcpy(outgoingMessage.pin, pin.c_str());

  esp_now_send(receiverMAC2, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
  Serial.printf("Mengirim pesan ke ESP2: %s\n", esp1Message.c_str());

  esp_now_send(receiverMAC3, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
  Serial.printf("Mengirim pesan ke ESP3: %s\n", esp1Message.c_str());

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
