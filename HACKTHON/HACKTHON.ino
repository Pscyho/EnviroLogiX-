

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <DHT.h>


#define DHTPIN      D8
#define DHTTYPE     DHT22
#define SOIL_PIN    A0
#define LDR_PIN     A3
#define THRESHOLD   530 

// === WiFi Credentials ===
const char* ssid = "Gadigeppa";
const char* password = "hunterkiller";

// === Sensor and WebSocket Setup ===
DHT dht(DHTPIN, DHTTYPE);
WebSocketsServer webSocket(81);
WiFiServer server(80);

// === HTML Webpage ===
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>🌦️ Weather Monitoring Dashboard</title>
  <style>
    body {
      font-family: 'Segoe UI', sans-serif;
      background: linear-gradient(to right, #e0f7fa, #f1f8e9);
      color: #333;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 2rem;
      margin: 0;
    }

    h1 {
      font-size: 2.5rem;
      margin-bottom: 1rem;
      color: #00796B;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 1rem;
      width: 100%;
      max-width: 900px;
    }

    .card {
      background: #fff;
      border-radius: 1rem;
      box-shadow: 0 4px 20px rgba(0,0,0,0.1);
      padding: 1.5rem;
      text-align: center;
      transition: transform 0.2s ease;
    }

    .card:hover {
      transform: translateY(-4px);
    }

    .card h2 {
      margin: 0;
      font-size: 1.2rem;
      color: #616161;
    }

    .value {
      font-size: 2rem;
      font-weight: bold;
      margin-top: 0.5rem;
    }

    #riskStatus {
      margin-top: 2rem;
      font-size: 1.5rem;
      font-weight: bold;
      padding: 1rem 2rem;
      border-radius: 1rem;
      background: #e0f2f1;
      color: #00695C;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }

    #riskStatus.high {
      background: #ffebee;
      color: #c62828;
    }

    footer {
      margin-top: 3rem;
      font-size: 0.9rem;
      color: #777;
    }
  </style>
</head>
<body>
  <h1>🌤️ Live Weather Dashboard</h1>
  <div class="grid">
    <div class="card">
      <h2>🌡️ Temperature</h2>
      <div class="value" id="temp">-- °C</div>
    </div>
    <div class="card">
      <h2>💧 Humidity</h2>
      <div class="value" id="hum">-- %</div>
    </div>
    <div class="card">
      <h2>🌱 Soil Moisture</h2>
      <div class="value" id="soil">--</div>
    </div>
    <div class="card">
      <h2>☀️ Light Level</h2>
      <div class="value" id="ldr">--</div>
    </div>
  </div>

  <div id="riskStatus" class="">Status: --</div>

  <footer>📡 Real-time data via WebSocket | © 2025</footer>

  <script>
    const socket = new WebSocket(`ws://${location.hostname}:81/`);
    socket.onmessage = function(event) {
      const data = JSON.parse(event.data);
      document.getElementById("temp").textContent = data.temp + " °C";
      document.getElementById("hum").textContent = data.hum + " %";
      document.getElementById("soil").textContent = data.soil;
      document.getElementById("ldr").textContent = data.ldr;
      
      const riskElem = document.getElementById("riskStatus");
      riskElem.textContent = "Status: " + data.risk;

      if (data.risk.includes("High")) {
        riskElem.classList.add("high");
      } else {
        riskElem.classList.remove("high");
      }
    };
  </script>
</body>
</html>
)rawliteral";


void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();

  WiFiClient client = server.available();
  if (client) {
    Serial.println("Sending HTML page...");
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type: text/html");
    client.println();
    client.println(webpage);
    client.stop();
  }

  static unsigned long lastSend = 0;
  if (millis() - lastSend > 2000) {
    lastSend = millis();

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int soilRaw = analogRead(SOIL_PIN);
    int ldr = analogRead(LDR_PIN);

    String soilStatus = (soilRaw > THRESHOLD) ? "DRY (" + String(soilRaw) + ")" : "WET (" + String(soilRaw) + ")";

    String riskStatus = "✅ Normal Conditions";
    if (hum > 85.0 && soilRaw <= THRESHOLD && ldr < 500) {
      riskStatus = "⚠️ High Delivery Risk";
    }

    if (!isnan(temp) && !isnan(hum)) {
      String json = "{\"temp\":" + String(temp, 1) +
                    ",\"hum\":" + String(hum, 1) +
                    ",\"soil\":\"" + soilStatus +
                    "\",\"ldr\":" + String(ldr) +
                    ",\"risk\":\"" + riskStatus + "\"}";
      webSocket.broadcastTXT(json);
      Serial.println("Sent: " + json);
    } else {
      Serial.println("Failed to read from DHT sensor");
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.println("WebSocket client connected");
  } else if (type == WStype_DISCONNECTED) {
    Serial.println("WebSocket client disconnected");
  }
}