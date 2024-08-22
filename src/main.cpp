#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#define NUM_LEDS 4
#define LED_PIN 2
const char* ssid = "VSMI-Guest";
const char* password = "h3ll0vsmi";
// Set your Static IP address
IPAddress local_IP(192, 168, 1, 10);
// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 255, 0);
WebServer server(80);
WiFiUDP udp;
#define NUM_PIXELS 100
#define MAX_UDP_SIZE 1450
unsigned int localUdpPort = 5000;
const int bufferSize = NUM_PIXELS*2;  // Adjust based on your data size
char incomingPacket[bufferSize];  // Buffer for incoming packets
uint8_t tempBuffer[NUM_PIXELS * 2];
int receivedBytes = 0;  // Số byte đã nhận
//const size_t capacity = JSON_ARRAY_SIZE(1000) + 4*JSON_STRING_SIZE(6);  // Dung lượng cần thiết cho JSON
JsonDocument jsonDocument;
#pragma region HTML page
const char* htmlPage = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Led Strip Controller</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100vh;
            background-color: #f0f0f0;
        }

        .container {
            background: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        }
        .buttons {
            display: flex;
            justify-content: space-between;
            gap: 10px; /* Khoảng cách giữa các nút */
        }

        .buttons button {
            padding: 10px 20px;
            font-size: 16px;
            cursor: pointer;
            border: none;
            border-radius: 4px;
            background-color: #007bff;
            color: #fff;
            transition: background-color 0.3s;
        }

        .buttons button:hover {
            background-color: #0056b3;
        }
        .input-group input {
            padding: 10px;
            font-size: 16px;
            border: 1px solid #ccc;
            border-radius: 4px;
            width: 100%;
            box-sizing: border-box;
        }
        .checkbox-group {
            margin-bottom: 20px;
            display: flex;
            flex-direction: row;
        }

        .checkbox-group label {
            display: block;
            margin-bottom: 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="input-group">
            <label for="brightness">Brightness:</label>
            <input type="number" id="brightness" placeholder="Brightness">
        </div>
        <!-- <div class="input-group">
            <label for="speed">Speed:</label>
            <input type="number" id="speed" placeholder="Speed">
        </div> -->
        <div class="input-group">
            <label for="leds">LEDs:</label>
            <input type="number" id="leds" placeholder="LEDs">
        </div>
        <div class="buttons">
            <button onclick="sendInputData()">Apply Config</button>
            <button onclick="sendLeds()">Set LEDs</button>
        </div>
    </div>

    <script>
        function sendData(action) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/" + action, true);
            xhr.send();
        }
        function sendInputData() {
          var brightness = document.getElementById('brightness').value;
          //var speed = document.getElementById('speed').value;
          var xhr = new XMLHttpRequest();
          //xhr.open("GET", "/inputs?brightness=" + brightness + "&speed=" + speed+ "&leds=" + leds, true);
          xhr.open("GET", "/inputs?brightness=" + brightness, true);
          
          xhr.send();
        }
        function sendLeds() {
          var Leds = document.getElementById('leds').value;
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "/setLeds?leds=" + Leds, true);
          xhr.send();
        }
    </script>
</body>
</html>
)=====";
#pragma endregion
#pragma region LED Config
#define LED_TYPE NEOPIXEL
#define COLOR_ORDER RGB
CRGB leds[NUM_LEDS];
// Chuyển đổi giá trị HEX thành CRGB
CRGB hexToRGB(const String& hexStr) {
    uint32_t color;
    // Convert HEX String to uint32_t
    if (hexStr.length() == 6) {
        color = strtol(hexStr.c_str(), NULL, 16); // Chuyển đổi chuỗi HEX thành giá trị số
    } else {
        color = 0; // Nếu chuỗi không hợp lệ, trả về màu đen
    }
    
    return CRGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
}
CRGB rgb565ToCRGB(uint16_t rgb565) {
    // Tách các thành phần màu từ giá trị RGB565
    uint8_t red   = (rgb565 >> 11) & 0x1F;  // 5 bit đỏ
    uint8_t green = (rgb565 >> 5) & 0x3F;   // 6 bit xanh lá
    uint8_t blue  = rgb565 & 0x1F;          // 5 bit xanh dương

    // Mở rộng giá trị màu từ 5 bit hoặc 6 bit lên 8 bit
    red   = (red   * 255 + 15) / 31;   // Chuyển từ 5 bit lên 8 bit
    green = (green * 255 + 31) / 63;   // Chuyển từ 6 bit lên 8 bit
    blue  = (blue  * 255 + 15) / 31;   // Chuyển từ 5 bit lên 8 bit

    return CRGB(red, green, blue);
}
// Cập nhật LED strip từ mảng dữ liệu màu
// Cập nhật LED strip từ mảng chuỗi HEX (String)
void updateLEDsFromHexArray(JsonArray jsonArray) {
  int i = 0;
  for (JsonVariant value : jsonArray) {
    leds[i] = hexToRGB(value.as<String>());
    i++;
  }
    FastLED.show();
}
void updateLEDArray(int newLength) {
    FastLED.addLeds<LED_TYPE, LED_PIN>(leds, newLength); 
}

void receivePixels(char* packet, int length) {
  if (receivedBytes + length > sizeof(tempBuffer)) {
        length = sizeof(tempBuffer) - receivedBytes;  // Giới hạn độ dài
  }
  memcpy(tempBuffer + receivedBytes, packet, length);
  receivedBytes += length;
  updateLEDArray(receivedBytes); 
  // Kiểm tra xem toàn bộ dữ liệu có được nhận đầy đủ chưa
    for (int i = 0; i < receivedBytes; i++) {
            uint16_t color = (tempBuffer[i*2] << 8) | tempBuffer[i*2 + 1];
            leds[i] = rgb565ToCRGB(color);
        }
        FastLED.show();
        // Đặt lại bộ nhớ tạm và biến nhận bytes
        memset(tempBuffer, 0, sizeof(tempBuffer));
        receivedBytes = 0;
}
#pragma endregion

#pragma region WEB Config
char buffer[250];
void handleInput()
{
  String brightness = server.arg("brightness");
  // Chuyển đổi giá trị nhập thành số
  int value1 = brightness.toInt();

  // Thực hiện hành động dựa trên giá trị nhập
  Serial.printf("brightness: %d\n", value1);
  server.send(200);
}
void handleLeds()
{
  String Led = server.arg("leds");
  // Chuyển đổi giá trị nhập thành số
  int updateLEDs = Led.toInt();
  updateLEDArray(updateLEDs);
  Serial.printf("leds: %d\n", updateLEDs);
  server.send(200);
}
void handlePost() {

  if (server.hasArg("plain") == false) {

  }
  String body = server.arg("plain");

  deserializeJson(jsonDocument, body);
  JsonArray jsonArray = jsonDocument["data"].as<JsonArray>();;
  
  updateLEDsFromHexArray(jsonArray);
  server.send(200, "application/json", "{}");
}
void setup_routing() {     
  server.on("/data", HTTP_POST, handlePost);  
   // Cấu hình server
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });
  server.on("/inputs", []() {
    handleInput();
  });
   server.on("/setLeds", []() {
    handleLeds();
  });
  server.begin();    
}
#pragma endregion


void setup() 
{
  FastLED.addLeds<LED_TYPE, LED_PIN>(leds, NUM_LEDS); 
  Serial.begin(115200);
  if (!WiFi.config(local_IP, gateway, subnet)) {
  Serial.println("STA Failed to configure");
  }
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  udp.begin(localUdpPort);
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  setup_routing();  
  Serial.println(FastLED.count());
}
void loop() {
  server.handleClient(); 
  int packetSize = udp.parsePacket();
  if (packetSize) {
      int len = udp.read(incomingPacket, MAX_UDP_SIZE);

      //Serial.println(len);
      if (len > 0) {
          receivePixels(incomingPacket, len);
      }
  } else {
    //Serial.println("No packet received");
  }
	// leds[0] = CRGB::White; FastLED.show(); delay(30);
	// leds[0] = CRGB::Black; FastLED.show(); delay(30);
}