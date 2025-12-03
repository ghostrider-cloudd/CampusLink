#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//  NODE SETTINGS 
#define NODE_ID 3
const char* AP_SSID = "Class Room";
const char* AP_PASSWORD = "campus1234";

//  OLED SETTINGS (128x32) 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//  LORA SETTINGS 
#define LORA_SCK   5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_SS    18
#define LORA_RST   14
#define LORA_DIO0  26
#define LORA_FREQ  433E6

//  GLOBALS 
WebServer server(80);

String statusMessage = "";
bool newMessageFlag = false;

// Store only last 2 messages
#define MAX_MESSAGES 2
String msgHistory[MAX_MESSAGES];
int fromHistory[MAX_MESSAGES];
int msgCount = 0;

// OLED timer
unsigned long displayTimer = 0;
bool displayActive = false;

//  FUNCTION DECLARATIONS 
String buildPage();
void handleRoot();
void handleSend();
void checkLoRaReceive();
void showOnDisplay(uint8_t from, uint8_t to, String msg);
void clearDisplaySafe();

//  SETUP 
void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  // OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("CampusConnect");
  display.println("Class Room");
  display.println("Node 3 Ready");
  display.display();

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQ)) {
    while (1);
  }

  // Web server
  server.on("/", handleRoot);
  server.on("/send", handleSend);

  // Auto refresh
  server.on("/check", []() {
    if (newMessageFlag) {
      newMessageFlag = false;
      server.send(200, "text/plain", "1");
    } else {
      server.send(200, "text/plain", "0");
    }
  });

  server.begin();
}

//  LOOP 
void loop() {
  server.handleClient();
  checkLoRaReceive();

  if (displayActive && millis() - displayTimer > 3000) {
    clearDisplaySafe();
  }
}

//  WEB UI 
String buildPage() {
  String page =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>CampusConnect - Node 3</title>"

    "<style>"
      "body{margin:0;padding:0;font-family:Segoe UI,Roboto,sans-serif;background:#f4f6f8;color:#111827;}"
      ".wrapper{min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px;}"
      ".card{background:#fff;max-width:420px;width:100%;border-radius:14px;box-shadow:0 10px 25px rgba(0,0,0,0.1);padding:22px;}"
      ".title{font-size:20px;font-weight:600;margin-bottom:4px;}"
      ".badge{font-size:12px;background:#e0f2fe;color:#075985;padding:5px 12px;border-radius:999px;display:inline-block;margin-bottom:10px;}"
      ".subtitle{font-size:13px;color:#6b7280;margin-bottom:18px;}"
      "label{font-size:13px;font-weight:500;display:block;margin-bottom:6px;}"
      "select,input{width:100%;padding:11px;border-radius:10px;border:1px solid #d1d5db;margin-bottom:14px;font-size:14px;}"
      "button{width:100%;padding:12px;border:none;border-radius:999px;background:#2563eb;color:white;font-weight:600;font-size:14px;cursor:pointer;}"
      ".status{margin-top:12px;padding:10px;border-radius:10px;background:#ecfdf5;color:#166534;font-size:13px;border:1px solid #bbf7d0;}"
      ".message-card{margin-top:12px;padding:12px;border-radius:12px;background:#f9fafb;border:1px solid #e5e7eb;}"
      "table{width:100%;border-collapse:collapse;font-size:13px;margin-top:6px;}"
      "th,td{border:1px solid #d1d5db;padding:6px;text-align:left;}"
      "th{background:#e5e7eb;}"
    "</style>"

    "<script>"
      "function checkPasswordAndSend(e){"
        "e.preventDefault();"
        "let pwd = prompt('Enter 4-digit password:');"
        "if(pwd === null) return false;"
        "if(pwd === '1234'){"
          "document.getElementById('msgForm').submit();"
        "}else{"
          "alert('Wrong credentials');"
        "}"
        "return false;"
      "}"

      "setInterval(()=>{fetch('/check')"
      ".then(r=>r.text())"
      ".then(t=>{if(t==='1')location.reload();});},1000);"
    "</script>"

    "</head><body>"
    "<div class='wrapper'>"
      "<div class='card'>"
        "<div class='title'>CampusConnect</div>"
        "<div class='badge'>Node 3</div>"
        "<div class='subtitle'>LoRa Messaging Interface</div>"

        "<form id='msgForm' action='/send' onsubmit='return checkPasswordAndSend(event)'>"

          "<label>Destination</label>"
          "<select name='to'>"
            "<option value='1'>Admin</option>"
            "<option value='2'>Lab</option>"
          "</select>"

          "<label>User Name</label>"
          "<select name='sender' required>"
            "<option value='' disabled selected>-- Select User --</option>"
            "<option value='Liam'>Liam</option>"
            "<option value='Sophia'>Sophia</option>"
            "<option value='Noah'>Noah</option>"
            "<option value='Olivia'>Olivia</option>"
            "<option value='Ethan'>Ethan</option>"
          "</select>"

          "<label>Message</label>"
          "<input type='text' name='msg' maxlength='190' placeholder='Type your message...' required>"

          "<button type='submit'>Send Message</button>"
        "</form>";

  if (statusMessage != "") {
    page += "<div class='status'>" + statusMessage + "</div>";
  }

  if (msgCount > 0) {
    page += "<div class='message-card'>";
    page += "<div class='message-meta'>Recent Messages</div>";
    page += "<table><tr><th>From Node</th><th>Message</th></tr>";

    for (int i = msgCount - 1; i >= 0; i--) {
      page += "<tr>";
      page += "<td>" + String(fromHistory[i]) + "</td>";
      page += "<td>" + msgHistory[i] + "</td>";
      page += "</tr>";
    }

    page += "</table></div>";
  }

  page += "</div></div></body></html>";
  return page;
}

void handleRoot() {
  server.send(200, "text/html", buildPage());
}

//  SEND 
void handleSend() {
  String msg = server.arg("msg");
  int toNode = server.arg("to").toInt();
  String senderName = server.arg("sender");

  LoRa.beginPacket();
  LoRa.write((uint8_t)toNode);
  LoRa.write((uint8_t)NODE_ID);

  LoRa.print("From " + senderName + ": " + msg);

  LoRa.endPacket();

  statusMessage = "Message sent by " + senderName;

  showOnDisplay(NODE_ID, toNode, msg);

  server.sendHeader("Location", "/");
  server.send(302);
}

//  RECEIVE 
void checkLoRaReceive() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    uint8_t toNode   = LoRa.read();
    uint8_t fromNode = LoRa.read();

    String msg = "";
    while (LoRa.available()) {
      msg += (char)LoRa.read();
    }

    if (toNode == NODE_ID) {
      statusMessage = "New message received!";
      newMessageFlag = true;

      if (msgCount < MAX_MESSAGES) {
        fromHistory[msgCount] = fromNode;
        msgHistory[msgCount] = msg;
        msgCount++;
      } else {
        fromHistory[0] = fromHistory[1];
        msgHistory[0] = msgHistory[1];
        fromHistory[1] = fromNode;
        msgHistory[1] = msg;
      }

      showOnDisplay(fromNode, toNode, msg);
    }
  }
}

//  OLED 
void showOnDisplay(uint8_t from, uint8_t to, String msg) {
  display.clearDisplay();
  display.setCursor(0, 0);

  display.print("F:"); display.print(from);
  display.print(" T:"); display.println(to);

  if (msg.length() > 21) {
    msg = msg.substring(0, 21);
  }
  display.println(msg);
  display.display();

  displayTimer = millis();
  displayActive = true;
}

void clearDisplaySafe() {
  display.clearDisplay();
  display.display();
  displayActive = false;
}
