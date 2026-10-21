

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_camera.h"

// Wi-Fi
const char* WIFI_SSID = "your_wifi_name";
const char* WIFI_PASSWORD = "your_password";

// Python receiver PC IP address
IPAddress PC_IP(192, 168, 137, 1);
const uint16_t UDP_PORT = 5000;

// Camera pins: ESP32-S3 OV3660
#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   15
#define CAM_PIN_SIOD   4
#define CAM_PIN_SIOC   5

#define CAM_PIN_D0     11
#define CAM_PIN_D1     9
#define CAM_PIN_D2     8
#define CAM_PIN_D3     10
#define CAM_PIN_D4     12
#define CAM_PIN_D5     18
#define CAM_PIN_D6     17
#define CAM_PIN_D7     16

#define CAM_PIN_VSYNC  6
#define CAM_PIN_HREF   7
#define CAM_PIN_PCLK   13

WiFiUDP udp;

const uint16_t CHUNK_SIZE = 1200;
const uint16_t UDP_HEADER = 8;
const uint32_t FRAME_INTERVAL_MS = 50; // Target: 20 FPS

uint32_t frameId = 0;
uint32_t lastFrame = 0;

bool initCamera() {
  camera_config_t c = {};

  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;

  c.pin_pwdn = CAM_PIN_PWDN;
  c.pin_reset = CAM_PIN_RESET;
  c.pin_xclk = CAM_PIN_XCLK;
  c.pin_sccb_sda = CAM_PIN_SIOD;
  c.pin_sccb_scl = CAM_PIN_SIOC;

  c.pin_d0 = CAM_PIN_D0;
  c.pin_d1 = CAM_PIN_D1;
  c.pin_d2 = CAM_PIN_D2;
  c.pin_d3 = CAM_PIN_D3;
  c.pin_d4 = CAM_PIN_D4;
  c.pin_d5 = CAM_PIN_D5;
  c.pin_d6 = CAM_PIN_D6;
  c.pin_d7 = CAM_PIN_D7;

  c.pin_vsync = CAM_PIN_VSYNC;
  c.pin_href = CAM_PIN_HREF;
  c.pin_pclk = CAM_PIN_PCLK;

  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_QVGA;
  c.jpeg_quality = 14;

  bool psram = psramFound();
  c.fb_count = psram ? 2 : 1;
  c.fb_location = psram ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("Camera error: 0x%x\n", err);
    return false;
  }

  Serial.println("Camera OK");
  return true;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  udp.begin(5001);
  Serial.printf("UDP video target: %s:%u\n",
                PC_IP.toString().c_str(), UDP_PORT);
}

void sendFrame(camera_fb_t* fb) {
  // Header: frame ID (4 bytes), packet index (2), packet count (2).
  const uint16_t packetCount =
      (fb->len + CHUNK_SIZE - 1) / CHUNK_SIZE;

  // Our receiver uses a 16-bit packet count.
  if (packetCount == 0 || packetCount > 65535) return;

  for (uint16_t i = 0; i < packetCount; i++) {
    size_t offset = (size_t)i * CHUNK_SIZE;
    size_t chunkLen = fb->len - offset;

    if (chunkLen > CHUNK_SIZE) chunkLen = CHUNK_SIZE;

    uint8_t header[UDP_HEADER];

    header[0] = (frameId >> 24) & 0xFF;
    header[1] = (frameId >> 16) & 0xFF;
    header[2] = (frameId >> 8) & 0xFF;
    header[3] = frameId & 0xFF;

    header[4] = (i >> 8) & 0xFF;
    header[5] = i & 0xFF;

    header[6] = (packetCount >> 8) & 0xFF;
    header[7] = packetCount & 0xFF;

    if (!udp.beginPacket(PC_IP, UDP_PORT)) continue;

    udp.write(header, sizeof(header));
    udp.write(fb->buf + offset, chunkLen);

    if (!udp.endPacket()) {
      // A lost UDP packet means this frame may be discarded.
    }
  }

  frameId++;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!initCamera()) {
    Serial.println("Camera initialization failed");
    while (true) delay(1000);
  }

  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(100);
    return;
  }

  uint32_t now = millis();
  if (now - lastFrame < FRAME_INTERVAL_MS) return;
  lastFrame = now;

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return;

  sendFrame(fb);
  esp_camera_fb_return(fb);
}
