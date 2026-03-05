#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>


// --- CONFIGURACIÓN ---
const char *WIFI_SSID = "FAMILIA CV";
const char *WIFI_PASS = "FCVSara@1123";
const char *GEMINI_API_KEY = "AIzaSyByzMF8LT2x8GQneWD-bk0mD0UTRqio97k";

// URL de tu Firebase (ACTUALIZADA con tu Project ID: wlaz-cyberspy)
const char *FIREBASE_URL =
    "https://wlaz-cyberspy-default-rtdb.firebaseio.com/robot/chat.json";

// --- PANTALLA OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

String lastUserMsg = "";
bool isThinking = false;

// --- ROBOT FACE ---
void drawFace(String mode) {
  display.clearDisplay();
  if (mode == "thinking") {
    int offset = (millis() / 200) % 10 - 5;
    display.fillRoundRect(30 + offset, 20, 15, 15, 3, WHITE);
    display.fillRoundRect(80 + offset, 20, 15, 15, 3, WHITE);
  } else if (mode == "blink") {
    display.fillRect(30, 27, 20, 2, WHITE);
    display.fillRect(80, 27, 20, 2, WHITE);
  } else if (mode == "talking") {
    display.fillRoundRect(30, 20, 20, 20, 4, WHITE);
    display.fillRoundRect(80, 20, 20, 20, 4, WHITE);
    int h = (millis() / 100) % 10 + 2;
    display.fillRect(40, 50, 48, h, WHITE);
  } else {
    display.fillRoundRect(30, 20, 20, 20, 4, WHITE);
    display.fillRoundRect(80, 20, 20, 20, 4, WHITE);
    display.fillRect(45, 52, 38, 2, WHITE);
  }
  display.display();
}

String askGemini(String userMsg) {
  isThinking = true;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://generativelanguage.googleapis.com/v1/models/"
               "gemini-1.5-flash:generateContent?key=" +
               String(GEMINI_API_KEY);

  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    String prompt = "Eres Cyber-Espy PRO. Responde muy breve y cool (max 100 "
                    "caracteres): " +
                    userMsg;

    DynamicJsonDocument doc(1024);
    JsonArray contents = doc.createNestedArray("contents");
    JsonObject mainPart = contents.createNestedObject();
    JsonArray parts = mainPart.createNestedArray("parts");
    parts.createNestedObject()["text"] = prompt;

    String jsonPayload;
    serializeJson(doc, jsonPayload);
    int httpCode = http.POST(jsonPayload);

    if (httpCode == 200) {
      DynamicJsonDocument responseDoc(2048);
      deserializeJson(responseDoc, http.getString());
      http.end();
      isThinking = false;
      return responseDoc["candidates"][0]["content"]["parts"][0]["text"]
          .as<String>();
    }
    http.end();
  }
  isThinking = false;
  return "Error Mental...";
}

void uploadRespToFirebase(String resp) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, FIREBASE_URL);

  DynamicJsonDocument doc(512);
  doc["user_msg"] = lastUserMsg;
  doc["robot_resp"] = resp;
  doc["timestamp"] = millis();

  String payload;
  serializeJson(doc, payload);
  http.PATCH(payload); // Actualizamos solo la respuesta
  http.end();
}

void checkFirebase() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, FIREBASE_URL);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    String userMsg = doc["user_msg"].as<String>();
    String robotResp = doc["robot_resp"].as<String>();

    // Si hay un mensaje nuevo (la respuesta esta en "...")
    if (userMsg != "" && robotResp == "...") {
      lastUserMsg = userMsg;
      Serial.println("Nuevo comando recibido: " + userMsg);

      String aiResp = askGemini(userMsg);
      uploadRespToFirebase(aiResp);

      // Animacion de hablar
      unsigned long start = millis();
      while (millis() - start < 4000) {
        drawFace("talking");
      }
    }
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  Serial.println("Robot PRO Conectado a la Nube!");
}

void loop() {
  checkFirebase();

  if (isThinking)
    drawFace("thinking");
  else if ((millis() / 4000) % 10 == 0)
    drawFace("blink");
  else
    drawFace("idle");

  delay(1000); // Revisar Firebase cada segundo
}
