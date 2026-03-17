#include <WiFi.h>
#include <WiFiUdp.h>

WiFiUDP udp;

#define UDP_PORT 4210

// ==========================
// PIN DEFINITIONS (XIAO ESP32-C3)
// ==========================
#define A_IN1 2   // D0
#define A_IN2 3   // D1
#define A_ENA 4   // D2 PWM

#define B_IN1 5   // D3
#define B_IN2 6   // D4
#define B_ENB 7   // D5 PWM

// Motor state
int dutyA = 0, dirA = 0;
int dutyB = 0, dirB = 0;

// Failsafe
unsigned long lastPacketTime = 0;
const unsigned long FAILSAFE_TIMEOUT = 300;

// ==========================
// MOTOR CONTROL
// ==========================
void setMotor(int in1, int in2, int pwmPin, int speed, int dir)
{
  speed = constrain(speed, 0, 255);

  if (dir == 1) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  }
  else if (dir == -1) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  }
  else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }

  ledcWrite(pwmPin, speed);
}

void updateMotors()
{
  setMotor(A_IN1, A_IN2, A_ENA, dutyA, dirA);
  setMotor(B_IN1, B_IN2, B_ENB, dutyB, dirB);
}

// ==========================
// SETUP
// ==========================
void setup()
{
  Serial.begin(115200);

  pinMode(A_IN1, OUTPUT);
  pinMode(A_IN2, OUTPUT);
  pinMode(B_IN1, OUTPUT);
  pinMode(B_IN2, OUTPUT);

  // PWM (ESP32-C3 Core 3.x)
  ledcAttach(A_ENA, 25000, 8);
  ledcAttach(B_ENB, 25000, 8);

  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Rover", "12345678");

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // UDP
  udp.begin(UDP_PORT);
  Serial.println("UDP ready on port 4210");

  lastPacketTime = millis();
}

// ==========================
// LOOP
// ==========================
void loop()
{
  int packetSize = udp.parsePacket();

  if (packetSize)
  {
    char buf[64];
    int len = udp.read(buf, sizeof(buf) - 1);

    if (len > 0)
    {
      buf[len] = '\0';

      int sA, dA, sB, dB;

      if (sscanf(buf, "%d,%d,%d,%d", &sA, &dA, &sB, &dB) == 4)
      {
        dutyA = constrain(sA, 0, 255);
        dirA  = constrain(dA, -1, 1);

        dutyB = constrain(sB, 0, 255);
        dirB  = constrain(dB, -1, 1);

        updateMotors();
        lastPacketTime = millis();

        Serial.printf("A:%d,%d  B:%d,%d\n", dutyA, dirA, dutyB, dirB);
      }
    }
  }

  // FAILSAFE STOP
  if (millis() - lastPacketTime > FAILSAFE_TIMEOUT)
  {
    dutyA = dutyB = 0;
    dirA = dirB = 0;
    updateMotors();
  }
}