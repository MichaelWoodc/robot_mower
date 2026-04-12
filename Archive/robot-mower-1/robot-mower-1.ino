#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

// ===== MOTOR PINS =====

// Motor A
#define A_IN1 25
#define A_IN2 26
#define A_PWM 27

// Motor B
#define B_IN1 32
#define B_IN2 33
#define B_PWM 14

// ===== PWM SETTINGS =====
#define PWM_FREQ 20000
#define PWM_RES 8

// Motor A PWM state
unsigned long prevA = 0;
int dutyA = 0;
int dirA = 0;

// Motor B PWM state
unsigned long prevB = 0;
int dutyB = 0;
int dirB = 0;

// ===== SOFTWARE PWM FUNCTION =====
void runSoftPWM(int pin, unsigned long &prevMicros, int duty) {
  unsigned long interval = 1000000UL / PWM_FREQ;
  unsigned long highTime = (interval * duty) / 255;
  unsigned long now = micros();

  if (now - prevMicros >= interval) {
    prevMicros += interval;
  }

  if ((now - prevMicros) < highTime) {
    digitalWrite(pin, HIGH);
  } else {
    digitalWrite(pin, LOW);
  }
}

// ===== SET MOTOR DIRECTION =====
void setDirection(int in1, int in2, int dir) {
  if (dir == 1) {          // forward
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else if (dir == -1) {  // reverse
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {                 // stop
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }
}

// ===== HTTP HANDLERS (API) =====
void handleMotorA() {
  if (server.hasArg("speed")) dutyA = constrain(server.arg("speed").toInt(), 0, 255);
  if (server.hasArg("dir"))   dirA  = constrain(server.arg("dir").toInt(), -1, 1);
  setDirection(A_IN1, A_IN2, dirA);
  server.send(200, "text/plain", "OK A");
}

void handleMotorB() {
  if (server.hasArg("speed")) dutyB = constrain(server.arg("speed").toInt(), 0, 255);
  if (server.hasArg("dir"))   dirB  = constrain(server.arg("dir").toInt(), -1, 1);
  setDirection(B_IN1, B_IN2, dirB);
  server.send(200, "text/plain", "OK B");
}

// ===== TOUCHSCREEN UI (HTML) =====
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Tank Drive</title>
<style>
  body { margin:0; background:#111; color:#eee; font-family:sans-serif; display:flex; flex-direction:column; align-items:center; justify-content:center; height:100vh; }
  h1 { font-size:20px; margin:10px; }
  .container { display:flex; width:100%; height:80vh; justify-content:space-around; align-items:center; }
  .joystick {
    width:40vw; max-width:200px; height:60vh; max-height:400px;
    border:2px solid #555; border-radius:20px; position:relative;
    background:#222;
    touch-action:none;
  }
  .thumb {
    width:60px; height:60px; border-radius:50%;
    background:#0af; position:absolute;
    left:50%; transform:translateX(-50%);
    top:50%; margin-top:-30px;
  }
  .label { text-align:center; margin-top:5px; }
</style>
</head>
<body>
<h1>ESP32 Tank Drive</h1>
<div class="container">
  <div>
    <div id="joyA" class="joystick">
      <div id="thumbA" class="thumb"></div>
    </div>
    <div class="label">Left Motor (A)</div>
  </div>
  <div>
    <div id="joyB" class="joystick">
      <div id="thumbB" class="thumb"></div>
    </div>
    <div class="label">Right Motor (B)</div>
  </div>
</div>

<script>
function setupJoystick(joyId, thumbId, motor) {
  const joy = document.getElementById(joyId);
  const thumb = document.getElementById(thumbId);
  let active = false;
  let lastSend = 0;

  function send(speed, dir) {
    const now = Date.now();
    if (now - lastSend < 50) return; // limit to ~20Hz
    lastSend = now;
    const url = "/" + motor + "?speed=" + speed + "&dir=" + dir;
    fetch(url).catch(e => {});
  }

  function updateFromTouch(clientY) {
    const rect = joy.getBoundingClientRect();
    const centerY = rect.top + rect.height / 2;
    const dy = clientY - centerY;
    const half = rect.height / 2;

    let norm = -dy / half; // -1..1 (up positive)
    if (norm > 1) norm = 1;
    if (norm < -1) norm = -1;

    let dir = 0;
    let speed = 0;

    if (Math.abs(norm) < 0.1) {
      dir = 0;
      speed = 0;
    } else if (norm > 0) {
      dir = 1;
      speed = Math.round(norm * 255);
    } else {
      dir = -1;
      speed = Math.round(-norm * 255);
    }

    const thumbY = centerY + dy;
    thumb.style.top = (thumbY - rect.top - thumb.offsetHeight/2) + "px";

    send(speed, dir);
  }

  joy.addEventListener("touchstart", (e) => {
    active = true;
    updateFromTouch(e.touches[0].clientY);
  });

  joy.addEventListener("touchmove", (e) => {
    if (!active) return;
    e.preventDefault();
    updateFromTouch(e.touches[0].clientY);
  }, {passive:false});

  joy.addEventListener("touchend", (e) => {
    active = false;
    // center thumb and stop motor
    const rect = joy.getBoundingClientRect();
    thumb.style.top = (rect.height/2 - thumb.offsetHeight/2) + "px";
    send(0, 0);
  });

  // init thumb centered
  window.addEventListener("load", () => {
    const rect = joy.getBoundingClientRect();
    thumb.style.top = (rect.height/2 - thumb.offsetHeight/2) + "px";
  });
}

setupJoystick("joyA", "thumbA", "A");
setupJoystick("joyB", "thumbB", "B");
</script>
</body>
</html>
)rawliteral";

// ===== ROOT HANDLER =====
void handleRoot() {
  server.send_P(200, "text/html", MAIN_page);
}

void setup() {
  Serial.begin(115200);

  pinMode(A_IN1, OUTPUT);
  pinMode(A_IN2, OUTPUT);
  pinMode(A_PWM, OUTPUT);

  pinMode(B_IN1, OUTPUT);
  pinMode(B_IN2, OUTPUT);
  pinMode(B_PWM, OUTPUT);

  WiFi.softAP("ESP32-Robot", "12345678");
  Serial.println("AP started: ESP32-Robot / 12345678");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/A", handleMotorA);
  server.on("/B", handleMotorB);

  server.begin();
}

void loop() {
  server.handleClient();
  runSoftPWM(A_PWM, prevA, dutyA);
  runSoftPWM(B_PWM, prevB, dutyB);
}
