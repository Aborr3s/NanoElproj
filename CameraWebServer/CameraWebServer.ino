// =========================================================
//  ESP32-CAM  –  Fristående ansiktsdetektering
//  Ingen WiFi, ingen webbserver.
//  GPIO 12 = HIGH när ett ansikte detekteras (håller 3 sek).
// =========================================================

#include "esp_camera.h"
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"

// ---- Välj kameramodell ----
#define CAMERA_MODEL_AI_THINKER  // Has PSRAM
#include "camera_pins.h"

// ---- Inställningar ----
#define FACE_DETECT_PIN  12
#define FACE_HOLD_MS     5000   // Håller pinnen HIGH i 3 sekunder

// ---- Ansiktsdetektorer (skapas en gång) ----
static HumanFaceDetectMSR01 s1(0.1F, 0.5F, 10, 0.2F);
static HumanFaceDetectMNP01 s2(0.5F, 0.3F, 5);

static unsigned long lastFaceTime = 0;

// ----------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32-CAM ansiktsdetektering startar...");

  // GPIO-init
  pinMode(FACE_DETECT_PIN, OUTPUT);
  digitalWrite(FACE_DETECT_PIN, LOW);

  // Kamerakonfiguration – RGB565 + QVGA krävs för ansiktsdetektering
  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = Y2_GPIO_NUM;
  config.pin_d1        = Y3_GPIO_NUM;
  config.pin_d2        = Y4_GPIO_NUM;
  config.pin_d3        = Y5_GPIO_NUM;
  config.pin_d4        = Y6_GPIO_NUM;
  config.pin_d5        = Y7_GPIO_NUM;
  config.pin_d6        = Y8_GPIO_NUM;
  config.pin_d7        = Y9_GPIO_NUM;
  config.pin_xclk      = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync     = VSYNC_GPIO_NUM;
  config.pin_href      = HREF_GPIO_NUM;
  config.pin_sccb_sda  = SIOD_GPIO_NUM;
  config.pin_sccb_scl  = SIOC_GPIO_NUM;
  config.pin_pwdn      = PWDN_GPIO_NUM;
  config.pin_reset     = RESET_GPIO_NUM;
  config.xclk_freq_hz  = 20000000;
  config.pixel_format  = PIXFORMAT_RGB565;
  config.frame_size    = FRAMESIZE_QVGA;  // 320×240
  config.grab_mode     = CAMERA_GRAB_LATEST;
  config.fb_location   = CAMERA_FB_IN_PSRAM;
  config.fb_count      = 2;              // Dubbelbuffring

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera-init misslyckades: 0x%x\n", err);
    return;
  }

  Serial.println("Kamera redo!");
  Serial.printf("Ansiktsdetektering aktiv – GPIO %d = HIGH vid ansikte\n",
                FACE_DETECT_PIN);
}

// ----------------------------------------------------------
void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    delay(10);
    return;
  }

  // --- Tvåstegsdetektering (mer exakt) ---
  auto &candidates = s1.infer((uint16_t *)fb->buf,
                               {(int)fb->height, (int)fb->width, 3});
  auto &results    = s2.infer((uint16_t *)fb->buf,
                               {(int)fb->height, (int)fb->width, 3},
                               candidates);

  esp_camera_fb_return(fb);

  // --- GPIO-styrning ---
  if (results.size() > 0) {
    if (digitalRead(FACE_DETECT_PIN) == LOW) {
      Serial.printf("Ansikte detekterat! (%d st)\n", results.size());
    }
    digitalWrite(FACE_DETECT_PIN, HIGH);
    lastFaceTime = millis();
  } else if (millis() - lastFaceTime > FACE_HOLD_MS) {
    if (digitalRead(FACE_DETECT_PIN) == HIGH) {
      Serial.println("Inget ansikte – GPIO LOW");
    }
    digitalWrite(FACE_DETECT_PIN, LOW);
  }
}
