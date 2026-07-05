#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ====== AP credentials ======
const char* ap_ssid     = "ESP32-CAM";
const char* ap_password = "12345678";   // minimum 8 characters

// ====== Camera pins — ESP32-S3 N16R8 pinout ======
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5

#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11

#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

httpd_handle_t stream_httpd = NULL;

bool cameraOK = false;
esp_err_t cameraError = ESP_OK;

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* _STREAM_CONTENT_TYPE =
  "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;

static const char* _STREAM_BOUNDARY =
  "\r\n--" PART_BOUNDARY "\r\n";

static const char* _STREAM_PART =
  "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";


// ====== MJPEG stream handler ======
static esp_err_t stream_handler(httpd_req_t *req) {
  if (!cameraOK) {
    char msg[512];
    snprintf(msg, sizeof(msg),
      "<html><body style='background:#111;color:white;font-family:monospace;padding:20px'>"
      "<h2>Camera init failed</h2>"
      "<p>Error code: 0x%x</p>"
      "<p>PSRAM found: %s</p>"
      "<p>Check camera ribbon cable, power, and pin mapping.</p>"
      "</body></html>",
      cameraError,
      psramFound() ? "YES" : "NO"
    );

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, msg, strlen(msg));
  }

  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();

    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
      break;
    }

    res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));

    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    }

    esp_camera_fb_return(fb);
    fb = NULL;

    if (res != ESP_OK) {
      break;
    }

    // Important: give CPU/WiFi time, helps prevent hanging
    delay(10);
  }

  return res;
}


// ====== Web page ======
static esp_err_t index_handler(httpd_req_t *req) {
  const char* html =
    "<html>"
    "<head>"
    "<title>ESP32-S3 Camera</title>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "</head>"
    "<body style='margin:0;text-align:center;background:#000;color:white'>"
    "<h3>ESP32-S3 Camera Stream</h3>"
    "<img src='/stream' style='width:100%;max-width:800px'>"
    "</body>"
    "</html>";

  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, strlen(html));
}


// ====== Start HTTP server ======
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    "/",
    HTTP_GET,
    index_handler,
    NULL
  };

  httpd_uri_t stream_uri = {
    "/stream",
    HTTP_GET,
    stream_handler,
    NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("HTTP server started");
  } else {
    Serial.println("HTTP server start failed");
  }
}


void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("========== BOOTING ==========");

  // ====== PSRAM check ======
  if (psramFound()) {
    Serial.printf("PSRAM found: %u bytes\n", ESP.getPsramSize());
  } else {
    Serial.println("NO PSRAM DETECTED!");
    Serial.println("Set Arduino IDE: Tools > PSRAM > OPI PSRAM");
  }

  // ====== Start WiFi AP first ======
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  bool apStarted = WiFi.softAP(ap_ssid, ap_password);

  if (apStarted) {
    Serial.println("AP started");
    Serial.print("SSID: ");
    Serial.println(ap_ssid);
    Serial.print("Open: http://");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("AP start failed");
  }

  delay(300);

  // ====== Camera config ======
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0     = Y2_GPIO_NUM;
  config.pin_d1     = Y3_GPIO_NUM;
  config.pin_d2     = Y4_GPIO_NUM;
  config.pin_d3     = Y5_GPIO_NUM;
  config.pin_d4     = Y6_GPIO_NUM;
  config.pin_d5     = Y7_GPIO_NUM;
  config.pin_d6     = Y8_GPIO_NUM;
  config.pin_d7     = Y9_GPIO_NUM;

  config.pin_xclk   = XCLK_GPIO_NUM;
  config.pin_pclk   = PCLK_GPIO_NUM;
  config.pin_vsync  = VSYNC_GPIO_NUM;
  config.pin_href   = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn   = PWDN_GPIO_NUM;
  config.pin_reset  = RESET_GPIO_NUM;

  // More stable than 20 MHz on cheap boards
  config.xclk_freq_hz = 10000000;

  config.pixel_format = PIXFORMAT_JPEG;

  // Stable settings
  config.frame_size   = FRAMESIZE_VGA;     // 640x480
  config.jpeg_quality = 15;                // lower number = better quality, more memory
  config.fb_count     = 1;                 // more stable than 2
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.frame_size  = FRAMESIZE_QVGA;   // fallback if no PSRAM
    config.fb_count    = 1;
  }

  // ====== Init camera ======
  cameraError = esp_camera_init(&config);

  if (cameraError != ESP_OK) {
    Serial.printf("Camera init FAILED: 0x%x\n", cameraError);
    cameraOK = false;
  } else {
    Serial.println("Camera init SUCCESS");
    cameraOK = true;

    sensor_t *s = esp_camera_sensor_get();

    if (s != NULL) {
      Serial.printf("Sensor PID: 0x%x\n", s->id.PID);

      // OV3660 usually needs these
      s->set_vflip(s, 1);
      s->set_brightness(s, 1);
      s->set_saturation(s, -2);

      // Optional stability/image tuning
      s->set_gainceiling(s, GAINCEILING_2X);
    }
  }

  // ====== Start web server ======
  startCameraServer();

  Serial.println("========== READY ==========");
}


void loop() {
  static unsigned long lastPrint = 0;

  if (millis() - lastPrint > 10000) {
    lastPrint = millis();

    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());

    Serial.print(" | Free PSRAM: ");
    Serial.println(ESP.getFreePsram());
  }

  delay(1000);
}
