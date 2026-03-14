#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

// ===== MOTOR A PINS =====
#define A_IN1 19
#define A_IN2 18
#define A_ENA 17

// ===== MOTOR B PINS =====
#define B_IN1 25
#define B_IN2 26
#define B_ENB 27

// ===== WIFI =====
const char* ssid = "OpenWrt2.4";
const char* password = "narrowboat564";

// ===== MOTOR STATE =====
int dutyA = 0;
int dirA = 0;

int dutyB = 0;
int dirB = 0;

// ===== SET MOTOR DIRECTIONS =====
void setDirectionA(int dir) {
  if (dir == 1) { digitalWrite(A_IN1, HIGH); digitalWrite(A_IN2, LOW); }
  else if (dir == -1) { digitalWrite(A_IN1, LOW); digitalWrite(A_IN2, HIGH); }
  else { digitalWrite(A_IN1, LOW); digitalWrite(A_IN2, LOW); }
}

void setDirectionB(int dir) {
  if (dir == 1) { digitalWrite(B_IN1, HIGH); digitalWrite(B_IN2, LOW); }
  else if (dir == -1) { digitalWrite(B_IN1, LOW); digitalWrite(B_IN2, HIGH); }
  else { digitalWrite(B_IN1, LOW); digitalWrite(B_IN2, LOW); }
}

// ===== API HANDLERS =====
void handleMotorA() {
  dutyA = constrain(server.arg("speed").toInt(), 0, 255);
  dirA  = constrain(server.arg("dir").toInt(), -1, 1);

  setDirectionA(dirA);
  analogWrite(A_ENA, dutyA);

  server.send(200, "text/plain", "OK");
}

void handleMotorB() {
  dutyB = constrain(server.arg("speed").toInt(), 0, 255);
  dirB  = constrain(server.arg("dir").toInt(), -1, 1);

  setDirectionB(dirB);
  analogWrite(B_ENB, dutyB);

  server.send(200, "text/plain", "OK");
}

// ===== STATUS ENDPOINT =====
void handleStatus() {
  String json = "{";
  json += "\"dutyA\":" + String(dutyA) + ",";
  json += "\"dirA\":" + String(dirA) + ",";
  json += "\"dutyB\":" + String(dutyB) + ",";
  json += "\"dirB\":" + String(dirB);
  json += "}";
  server.send(200, "application/json", json);
}

// ===== HTML UI =====
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Tank Drive</title>
<style>
  body { margin:0; background:#111; color:#eee; font-family:sans-serif; text-align:center; }
  .container { display:flex; justify-content:space-around; margin-top:20px; }
  .joystick { width:40vw; max-width:200px; height:60vh; max-height:400px; border:2px solid #555; border-radius:20px; background:#222; position:relative; touch-action:none; }
  .thumb { width:60px; height:60px; border-radius:50%; background:#0af; position:absolute; left:50%; transform:translateX(-50%); }
  #statusBox { margin-top:20px; font-size:20px; }
</style>
</head>
<body>
<h1>ESP32 Tank Drive</h1>

<div class="container">
  <div>
    <div id="joyA" class="joystick"><div id="thumbA" class="thumb"></div></div>
    <div>Left Motor (A)</div>
  </div>
  <div>
    <div id="joyB" class="joystick"><div id="thumbB" class="thumb"></div></div>
    <div>Right Motor (B)</div>
  </div>
</div>

<div id="statusBox">
  A: <span id="aSpeed">0</span> / <span id="aDir">0</span><br>
  B: <span id="bSpeed">0</span> / <span id="bDir">0</span>
</div>

<script>
function setupJoystick(joyId, thumbId, motor) {
  const joy = document.getElementById(joyId);
  const thumb = document.getElementById(thumbId);
  let active = false, lastSend = 0;

  function send(speed, dir) {
    const now = Date.now();
    if (now - lastSend < 50) return;
    lastSend = now;
    fetch("/" + motor + "?speed=" + speed + "&dir=" + dir);
  }

  function updateTouch(y) {
    const r = joy.getBoundingClientRect();
    const center = r.top + r.height/2;
    const dy = y - center;
    const half = r.height/2;

    let norm = -dy / half;  // -1 to 1
    norm = Math.max(-1, Math.min(1, norm));

    let dir = 0;
    let speed = 0;

    // DEADZONE ±15%
    if (Math.abs(norm) < 0.15) {
      dir = 0;
      speed = 0;
    } else {
      dir = norm > 0 ? 1 : -1;

      // SCALE: 60 → 255
      let mag = Math.abs(norm);
      speed = Math.round(60 + (mag * (255 - 60)));
    }

    thumb.style.top = (y - r.top - 30) + "px";
    send(speed, dir);
  }

  joy.addEventListener("touchstart", e => { active = true; updateTouch(e.touches[0].clientY); });
  joy.addEventListener("touchmove", e => { if (active) { e.preventDefault(); updateTouch(e.touches[0].clientY); }}, {passive:false});
  joy.addEventListener("touchend", () => {
    active = false;
    const r = joy.getBoundingClientRect();
    thumb.style.top = (r.height/2 - 30) + "px";
    send(0,0);
  });

  window.addEventListener("load", () => {
    const r = joy.getBoundingClientRect();
    thumb.style.top = (r.height/2 - 30) + "px";
  });
}

setupJoystick("joyA", "thumbA", "A");
setupJoystick("joyB", "thumbB", "B");

setInterval(() => {
  fetch("/status").then(r => r.json()).then(d => {
    document.getElementById("aSpeed").textContent = d.dutyA;
    document.getElementById("aDir").textContent = d.dirA;
    document.getElementById("bSpeed").textContent = d.dutyB;
    document.getElementById("bDir").textContent = d.dirB;
  });
}, 100);
</script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send_P(200, "text/html", MAIN_page); }

void setup() {
  Serial.begin(115200);

  pinMode(A_IN1, OUTPUT);
  pinMode(A_IN2, OUTPUT);
  pinMode(A_ENA, OUTPUT);

  pinMode(B_IN1, OUTPUT);
  pinMode(B_IN2, OUTPUT);
  pinMode(B_ENB, OUTPUT);

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
  server.on("/A", handleMotorA);
  server.on("/B", handleMotorB);
  server.on("/status", handleStatus);

  server.begin();
}

void loop() {
  server.handleClient();
}
