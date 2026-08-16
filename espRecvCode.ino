// Receiver code
#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP_Mail_Client.h>
#include <ThingSpeak.h>
#include <HTTPClient.h>
// OLED setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Wi-Fi credentials
const char* ssid = "GalaxyM51";
const char* password = "n*******";

// ThingSpeak
WiFiClient client;
const char* myWriteAPIKey = "replace";
unsigned long myChannelNumber = replace;  //

// Email setup
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "replace"
#define AUTHOR_PASSWORD "replace"
#define RECIPIENT_EMAIL "replace"

SMTPSession smtp;

// Global vars
String incomingMessage = "";
bool wifiNeeded = false;

void displayMessage(String msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("Alert Received:");
  display.setCursor(0, 30);
  display.println(msg);
  display.display();
}

void sendEmail(String subject, String body) {
  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  SMTP_Message message;
  message.sender.name = "ESP32";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = subject;
  message.addRecipient("User", RECIPIENT_EMAIL);
  message.text.content = body.c_str();

  if (!smtp.connect(&session)) {
    Serial.println("✉️ Failed to connect to mail server.");
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("✉️ Error sending email: " + smtp.errorReason());
  } else {
    Serial.println("✉️ Email sent!");
  }

  smtp.closeSession();
}

void sendToThingSpeak(String msg) {
  ThingSpeak.begin(client);

  float field1 = 0.0;
  float field2 = 0.0;

  if (msg == "Cheetah detected!") {
    field1 = 1.0;
    field2 = 0.0;
  } else if (msg == "Elephant detected!") {
    field1 = 0.0;
    field2 = 1.0;
  }

  ThingSpeak.setField(1, field1);
  ThingSpeak.setField(2, field2);

  int status = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  if (status == 200) {
    Serial.println("📈 ThingSpeak update success.");
  } else {
    Serial.println("📉 ThingSpeak update failed. HTTP error: " + String(status));
  }
}

// Callback for ESP-NOW
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  char msg[len + 1];
  memcpy(msg, data, len);
  msg[len] = '\0';

  incomingMessage = String(msg);
  Serial.print("📩 Received message: ");
  Serial.println(incomingMessage);

  if (incomingMessage == "Cheetah detected!" || incomingMessage == "Elephant detected!") {
    wifiNeeded = true;
  }

  displayMessage(incomingMessage);
}

void switchToWiFi() {
  if (esp_now_deinit() == ESP_OK) {
    Serial.println("🔌 ESP-NOW deinit success.");
  }

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("📡 Connecting to Wi-Fi...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected!");
  } else {
    Serial.println("\n❌ Wi-Fi failed!");
  }
}

void reinitESPNow() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onDataRecv);
    Serial.println("🔄 ESP-NOW reinitialized.");
  } else {
    Serial.println("❌ ESP-NOW reinit failed!");
  }
}

void setup() {
  Serial.begin(115200);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED failed.");
    while (1);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println("📟 ESP32 Booting...");
  display.display();

  // Start ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() == ESP_OK) {
    Serial.println("📡 ESP-NOW started");
    esp_now_register_recv_cb(onDataRecv);
  } else {
    Serial.println("❌ ESP-NOW init failed!");
    ESP.restart();
  }
}

void loop() {
  if (wifiNeeded) {
    Serial.println("🔁 Switching to Wi-Fi...");
    switchToWiFi();
    sendEmail("🦁 Alert: Animal Detected", incomingMessage);
    sendToThingSpeak(incomingMessage);
    wifiNeeded = false;
    reinitESPNow();
  }

  delay(500);
}
