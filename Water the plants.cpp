#include <WiFi.h>
#include <PubSubClient.h> // 
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <DHT.h>

// --- ตั้งค่า Wi-Fi ---
const char* ssid = "LAPTOP-J68MSQUQ";
const char* password = "56123402";

// --- ตั้งค่า MQTT (ใช้ Broker สาธารณะฟรี) ---
const char* mqtt_server = "broker.hivemq.com"; 
const char* topic_soil = "miniproject/soil_020"; 
const char* topic_pump = "miniproject/pump_020";
const char* topic_btn  = "miniproject/btn_020"; 

// --- ตั้งค่าอุปกรณ์ ---
#define SOIL_PIN 34
#define DHTPIN 14
#define LED_PIN 27
#define DHTTYPE DHT11

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_SH1107 display = Adafruit_SH1107(128, 128, &Wire);
DHT dht(DHTPIN, DHTTYPE);

unsigned long lastMsg = 0;
bool pumpState = false;
bool manualMode = false;
int threshold = 40;

// ฟังก์ชันรับข้อความจากหน้าเว็บ (Callback)
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.print("Message arrived ["); Serial.print(topic); Serial.print("] "); Serial.println(msg);

  // ตรวจสอบคำสั่งที่ส่งมาจากหน้าเว็บ
  if (String(topic) == topic_btn) {
    manualMode = true; // เข้าโหมด Manual
    if (msg == "ON") pumpState = true;
    else if (msg == "OFF") pumpState = false;
    else if (msg == "AUTO") manualMode = false; // กลับสู่โหมด Auto
    
    digitalWrite(LED_PIN, pumpState ? HIGH : LOW);
    
    // อัปเดตสถานะกลับไปบอกหน้าเว็บ
    client.publish(topic_pump, pumpState ? "ON" : "OFF");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    // สร้าง Client ID สุ่ม
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Subscribe รอรับคำสั่งจากปุ่มกด
      client.subscribe(topic_btn);
    } else {
      Serial.print("failed, rc="); Serial.print(client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  delay(1000);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  if(!display.begin(0x3C, true)) { Serial.println(F("SH1107 failed")); for(;;); }
  display.clearDisplay();
  display.setRotation(1);
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);

  dht.begin();
  
  // เชื่อมต่อ WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi OK");

  // ตั้งค่า MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000) { // ส่งข้อมูลทุก 2 วินาที
    lastMsg = now;
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    int analogVal = analogRead(SOIL_PIN);
    int moisture = map(analogVal, 4095, 0, 0, 100);
    moisture = constrain(moisture, 0, 100);

    // ระบบอัตโนมัติ
    if (!manualMode) {
      if (moisture < threshold) pumpState = true;
      else pumpState = false;
      digitalWrite(LED_PIN, pumpState ? HIGH : LOW);
    }

    // ส่งค่าขึ้น MQTT Broker (ส่งเป็น String)
    String soilMsg = String(moisture);
    client.publish(topic_soil, soilMsg.c_str());
    client.publish(topic_pump, pumpState ? "ON" : "OFF");

    // แสดงผลจอ OLED
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("MQTT: Connected");
    display.print("Topic: .../soil_020"); // โชว์ Topic ให้รู้
    display.setCursor(0, 30);
    display.setTextSize(2);
    display.print("Soil: "); display.print(moisture); display.println("%");
    display.setTextSize(1);
    display.setCursor(0, 55);
    display.print("Pump: "); display.println(pumpState ? "ON" : "OFF");
    display.setCursor(0, 70);
    display.print("Mode: "); display.println(manualMode ? "MANUAL" : "AUTO");
    display.display();
  }
}