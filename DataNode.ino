#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <LoRa.h>

//  ADMIN NODE SETTINGS 
#define NODE_ID 99

const char* AP_SSID = "LogNode";
const char* AP_PASSWORD = "campus1234";

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

#define MAX_MESSAGES 15
String msgHistory[MAX_MESSAGES];
int fromHistory[MAX_MESSAGES];
int toHistory[MAX_MESSAGES];
String timeHistory[MAX_MESSAGES];  // HH:MM:SS timestamps
int msgCount = 0;

bool newMessageFlag = false;
bool triggerAutoDownload = false;

String pendingTimestamp = "";  // timestamp sent from browser

//  FUNCTION DECLARATIONS 
String buildPage();
void handleRoot();
void checkLoRaReceive();
String generateLogFile();

//  SETUP 
void setup() {
  Serial.begin(115200);

  // Create AP only (hotspot)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.println("Admin Dashboard at:");
  Serial.println("http://192.168.4.1/");

  // LoRa init
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa init failed!");
    while (1);
  }

  server.on("/", handleRoot);

  // Browser checks for new messages
  server.on("/check", []() {
    if (newMessageFlag) {
      newMessageFlag = false;
      server.send(200, "text/plain", "1");
    } else {
      server.send(200, "text/plain", "0");
    }
  });

  // Browser sends timestamp
  server.on("/timestamp", [](){
    pendingTimestamp = server.arg("t");
    server.send(200, "text/plain", "OK");
  });

  // File download
  server.on("/download", []() {
    server.sendHeader("Content-Type", "text/plain");
    server.sendHeader("Content-Disposition", "attachment; filename=LogNode.txt");
    server.send(200, "text/plain", generateLogFile());
  });

  server.begin();
}

//  LOOP 
void loop() {
  server.handleClient();
  checkLoRaReceive();
}

// ----------- BUILD HTML PAGE -
String buildPage() {

  String page =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
    "<title>Admin LogNode</title>"

    "<style>"
    "body{font-family:Arial;background:#f4f6f8;margin:0;padding:0;}"
    ".container{max-width:750px;margin:auto;padding:20px;}"
    "h2{text-align:center;color:#333;}"
    "table{width:100%;border-collapse:collapse;margin-top:20px;}"
    "th,td{border:1px solid #ccc;padding:8px;font-size:14px;}"
    "th{background:#ddd;}"
    ".btn{background:#4CAF50;color:white;padding:10px 18px;border:none;"
    "border-radius:5px;cursor:pointer;margin-top:15px;}"
    "</style>"

    "<script>"
    "function sendTime(){"
    "  let now=new Date();"
    "  let t=now.toLocaleTimeString();"
    "  fetch('/timestamp?t='+t);"
    "}"

    "setInterval(()=>{"
    " fetch('/check').then(r=>r.text()).then(t=>{"
    "   if(t==='1'){ sendTime(); setTimeout(()=>location.reload(),300); }"
    " });"
    "},1000);"
    "</script>"

    "</head><body>"
    "<div class='container'>"
    "<h2>Admin Log Dashboard</h2>"
    "<button class='btn' onclick=\"window.location='/download'\">Download Messages</button>"

    "<table><tr><th>Time</th><th>From</th><th>To</th><th>Message</th></tr>";

  for (int i = 0; i < msgCount; i++) {
    page += "<tr>";
    page += "<td>" + timeHistory[i] + "</td>";
    page += "<td>" + String(fromHistory[i]) + "</td>";
    page += "<td>" + String(toHistory[i]) + "</td>";
    page += "<td>" + msgHistory[i] + "</td>";
    page += "</tr>";
  }

  page += "</table></div></body></html>";
  return page;
}

// ----------- ROUTE -
void handleRoot() {
  server.send(200, "text/html", buildPage());
}

// ----------- GENERATE FILE -
String generateLogFile() {
  String file = "===== LogNode Message Log =====\n\n";

  for (int i = 0; i < msgCount; i++) {
    file += "[" + timeHistory[i] + "]";
    file += " From: " + String(fromHistory[i]);
    file += " | To: " + String(toHistory[i]);
    file += " | Msg: " + msgHistory[i] + "\n";
  }

  return file;
}

// ----------- LORA RECEIVE -
void checkLoRaReceive() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  uint8_t toNode   = LoRa.read();
  uint8_t fromNode = LoRa.read();

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  Serial.println("MSG RECEIVED: " + msg);

  // No timestamp yet → use placeholder until browser sends it
  String timestamp = pendingTimestamp != "" ? pendingTimestamp : "00:00:00";
  pendingTimestamp = "";

  // Shift older entries DOWN (newest first)
  for (int i = MAX_MESSAGES - 1; i > 0; i--) {
    timeHistory[i] = timeHistory[i-1];
    fromHistory[i] = fromHistory[i-1];
    toHistory[i]   = toHistory[i-1];
    msgHistory[i]  = msgHistory[i-1];
  }

  // Insert newest at top
  timeHistory[0] = timestamp;
  fromHistory[0] = fromNode;
  toHistory[0]   = toNode;
  msgHistory[0]  = msg;

  if (msgCount < MAX_MESSAGES) msgCount++;

  newMessageFlag = true;
}
