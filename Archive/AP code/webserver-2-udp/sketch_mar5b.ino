#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>

WebServer server(80);
WiFiUDP udp;

#define UDP_PORT 4210

// Motor pins
#define A_IN1 19
#define A_IN2 18
#define A_ENA 17

#define B_IN1 25
#define B_IN2 26
#define B_ENB 27

// Motor state
int dutyA = 0, dirA = 0;
int dutyB = 0, dirB = 0;

// Failsafe timer
unsigned long lastPacketTime = 0;
const unsigned long FAILSAFE_TIMEOUT = 300; // ms

// ==========================
// MOTOR CONTROL
// ==========================
void setMotor(int in1, int in2, int ena, int speed, int dir) {
  speed = constrain(speed, 0, 255);

  if (dir == 1) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else if (dir == -1) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }

  analogWrite(ena, speed);
}

void updateMotors() {
  setMotor(A_IN1, A_IN2, A_ENA, dutyA, dirA);
  setMotor(B_IN1, B_IN2, B_ENB, dutyB, dirB);
}

// ==========================
// HTTP HANDLERS
// ==========================
void handleStatus() {
  String json = "{";
  json += "\"A_speed\":" + String(dutyA) + ",";
  json += "\"A_dir\":" + String(dirA) + ",";
  json += "\"B_speed\":" + String(dutyB) + ",";
  json += "\"B_dir\":" + String(dirB) + ",";
  json += "\"RSSI\":" + String(WiFi.RSSI());
  json += "}";
  server.send(200, "application/json", json);
}

void handleTest() {
  String cmd = server.arg("cmd");

  if (cmd == "fwd") { dutyA = dutyB = 255; dirA = dirB = 1; }
  else if (cmd == "rev") { dutyA = dutyB = 255; dirA = dirB = -1; }
  else if (cmd == "left") { dutyA = 255; dirA = -1; dutyB = 255; dirB = 1; }
  else if (cmd == "right") { dutyA = 255; dirA = 1; dutyB = 255; dirB = -1; }
  else { dutyA = dutyB = 0; dirA = dirB = 0; }

  updateMotors();
  server.send(200, "text/plain", "OK");
}

// ==========================
// SETUP
// ==========================
void setup() {
  Serial.begin(115200);

  pinMode(A_IN1, OUTPUT);
  pinMode(A_IN2, OUTPUT);
  pinMode(A_ENA, OUTPUT);

  pinMode(B_IN1, OUTPUT);
  pinMode(B_IN2, OUTPUT);
  pinMode(B_ENB, OUTPUT);

  // Start AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Rover", "12345678");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Start UDP
  udp.begin(UDP_PORT);
  Serial.println("UDP listening on port 4210");

  // HTTP routes
  server.on("/status", handleStatus);
  server.on("/test", handleTest);
  server.begin();

  lastPacketTime = millis();
}

// ==========================
// LOOP
// ==========================
void loop() {
  server.handleClient();

  // Handle UDP packets
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buf[64];
    int len = udp.read(buf, sizeof(buf) - 1);
    if (len > 0) {
      buf[len] = '\0';

      int sA, dA, sB, dB;
      if (sscanf(buf, "%d,%d,%d,%d", &sA, &dA, &sB, &dB) == 4) {
        dutyA = constrain(sA, 0, 255);
        dirA  = constrain(dA, -1, 1);
        dutyB = constrain(sB, 0, 255);
        dirB  = constrain(dB, -1, 1);

        updateMotors();
        lastPacketTime = millis();  // update failsafe timer
      }
    }
  }

  // FAILSAFE: stop if no UDP packets recently
  if (millis() - lastPacketTime > FAILSAFE_TIMEOUT) {
    dutyA = dutyB = 0;
    dirA = dirB = 0;
    updateMotors();
  }
}
