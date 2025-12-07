#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>

// ================= WiFi =================
#define WIFI_SSID "iPhone 9012"
#define WIFI_PASS "010203zXc"

// ============== LINE Messaging ==============
const char* LINE_CHANNEL_TOKEN = "H6UBRcIbyHLUTEyYjJUYg8BGi3bTwBBVvFEtjys0SXeN7emkFCO1qLDWV4Sh+UUVzOe8HEU2bS0gL7DJEmoX6P2Sa16cmc9nvoJOnJL9CXM87yHhVkjuzU1AF7CYXeGVT8GnF3JbNfCX1wi0n781hQdB04t89/1O/w1cDnyilFU=";  // แก้เป็นของจริง
const char* LINE_TARGET_ID     = "C64cab14d3d6a0b09b58a16bd99ee862f"; // หรือ U6f5495ca6b70d976f98ee4576f37c136

// ============== UART กับ STM32 ==============
HardwareSerial STMSerial(1);  // ใช้ Serial1

// แก้สองค่าด้านล่างให้ตรงกับขาที่คุณต่อ RX/TX ของ ESP32-S3
const int STM_RX_PIN = 18;    // ขา RX ของ ESP32 (รับจาก TX ของ STM32)
const int STM_TX_PIN = 17;    // ขา TX ของ ESP32 (ส่งไป RX ของ STM32)
const long STM_BAUD  = 115200;

// buffer เก็บข้อความจาก STM32 จนจบ 1 บรรทัด
String stmBuffer;

// ================= ฟังก์ชันช่วย =================

void connectWiFi() {
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// type ง่าย ๆ สำหรับแจ้งเตือน
enum AlertType {
  ALERT_SOUND,
  ALERT_OBJECT,
  ALERT_BOTH
};

// ส่งข้อความ + (option) สติ๊กเกอร์ไป LINE
void sendLineAlert(uint8_t type) {
  // เตรียมข้อความตามเหตุการณ์
  String text;
  String packageId;
  String stickerId;

  switch (type) {
    case ALERT_SOUND:
      text = "แจ้งเตือน: มีเสียงดังผิดปกติ";
      packageId = "11537";
      stickerId = "52002749";   // สติ้กเกอร์ A (กรณีเสียงอย่างเดียว)
      break;

    case ALERT_OBJECT:
      text = "แจ้งเตือน: ตรวจพบวัตถุผ่าน";
      packageId = "11538";
      stickerId = "51626511";   // สติ้กเกอร์ B (กรณีมีวัตถุ)
      break;

    case ALERT_BOTH:
      text = "แจ้งเตือน: มีเสียงดังและมีวัตถุผ่านพร้อมกัน!";
      packageId = "11537";
      stickerId = "52002756";   // สติ้กเกอร์ C (กรณีหนักสุด)
      break;
  }

  WiFiClientSecure client;
  client.setInsecure();  // ข้าม cert check เพื่อให้ใช้บน ESP32 ง่าย

  HTTPClient https;
  const char* url = "https://api.line.me/v2/bot/message/push";

  if (!https.begin(client, url)) {
    Serial.println("HTTPS begin() failed");
    return;
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", String("Bearer ") + LINE_CHANNEL_TOKEN);

  // JSON: ข้อความ + สติ้กเกอร์ (ตามเคส)
  String body = String("{")
    + "\"to\":\"" + LINE_TARGET_ID + "\","
    + "\"messages\":["
      "{"
        "\"type\":\"text\","
        "\"text\":\"" + text + "\""
      "},"
      "{"
        "\"type\":\"sticker\","
        "\"packageId\":\"" + packageId + "\","
        "\"stickerId\":\"" + stickerId + "\""
      "}"
    "]"
  "}";

  Serial.println("Request body:");
  Serial.println(body);

  int httpCode = https.POST(body);
  Serial.print("HTTP status code from LINE = ");
  Serial.println(httpCode);

  String payload = https.getString();
  Serial.println("Response body:");
  Serial.println(payload);

  https.end();
}

// แยกคำสั่งจาก STM แล้วเลือกแจ้งเตือน
void handleSTMCommand(const String& rawCmd) {
  String cmd = rawCmd;
  cmd.trim();  // ตัด \r\n ช่องว่าง หน้า-หลัง

  Serial.print("Got command from STM32: [");
  Serial.print(cmd);
  Serial.println("]");

  if (cmd == "ALARM_NOISE") {
    sendLineAlert(ALERT_SOUND);
  } else if (cmd == "ALARM_OBSTACLE") {
    sendLineAlert(ALERT_OBJECT);
  } else if (cmd == "ALARM_BOTH") {
    sendLineAlert(ALERT_BOTH);
  } else {
    Serial.println("Unknown command from STM32");
  }
}

// ================= setup / loop =================

void setup() {
  Serial.begin(115200);
  delay(1000);

  // UART ที่ต่อกับ STM32
  STMSerial.begin(STM_BAUD, SERIAL_8N1, STM_RX_PIN, STM_TX_PIN);
  Serial.println("STM32 UART ready.");

  connectWiFi();

  Serial.println("System ready. Waiting for STM32 commands...");
    // 🔹 ทดสอบยิง LINE ตรง ๆ 3 เคส
  // sendLineAlert(ALERT_SOUND);
  // delay(2000);
  // sendLineAlert(ALERT_OBJECT);
  // delay(2000);
  // sendLineAlert(ALERT_BOTH);

}

void loop() {
  // อ่านข้อมูลจาก STM32 ทีละตัวอักษร จนกว่าจะเจอ \n แล้วค่อย parse ทีเดียว
  while (STMSerial.available() > 0) {
    char c = STMSerial.read();

    if (c == '\n') {
      if (stmBuffer.length() > 0) {
        handleSTMCommand(stmBuffer);
        stmBuffer = "";  // เคลียร์ buffer
      }
    } else if (c != '\r') {
      // ไม่เก็บ \r
      stmBuffer += c;
    }
  }

  // ตรงนี้ถ้าอยากทำอย่างอื่นเพิ่มก็ใส่ได้
}
