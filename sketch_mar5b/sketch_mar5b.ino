#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

// ===== MOTOR A PINS =====
#define PIN_IN1 19
#define PIN_IN2 18
#define PIN_ENA 17

// ===== WIFI =====
const char* ssid = "OpenWrt2.4";
const char* password = "narrowboat564";

// ===== RAMP STATE =====
int pwmValue = 0;
unsigned long lastStep = 0;
int stepInterval = 2000;   // 2 seconds
int stepAmount = 10;       // increase by 10 each step

// ===== SET MOTOR DIRECTION =====
void setDirectionA(int dir) {
  if (dir == 1) { digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW); }
  else if (dir == -1) { digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, HIGH); }
  else { digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW); }
}

// ===== SIMPLE STATUS PAGE =====
void handleRoot() {
  String html = "<html><body style='font-family:sans-serif;'>";
  html += "<h2>Motor A PWM Ramp Test</h2>";
  html += "<p><b>Current PWM:</b> " + String(pwmValue) + "</p>";
  html += "<p><b>Direction:</b> Forward</p>";
  html += "<p><b>Next step in:</b> 2 seconds</p>";
  html += "<p>Motor increases PWM by +10 every 2 seconds until reset.</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_ENA, OUTPUT);

  // Always forward for this test
  setDirectionA(1);

  // ===== WIFI =====
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();

  unsigned long now = millis();

  // Every 2 seconds, increase PWM
  if (now - lastStep >= stepInterval) {
    lastStep = now;

    pwmValue += stepAmount;
    if (pwmValue > 255) pwmValue = 0;  // wrap around

    analogWrite(PIN_ENA, pwmValue);

    Serial.print("PWM = ");
    Serial.println(pwmValue);
  }
}
