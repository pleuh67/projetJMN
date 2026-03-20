#include "mesures.h"
#include <math.h>

// ── I2S / INMP441 ─────────────────────────────────────────────────────────────
// ⚠️ Conflit GPIO1-3 avec Wio-SX1262 sur XIAO ESP32S3.
//    Activer -DUSE_INMP441 uniquement sur carte sans module LoRa.

#ifdef USE_INMP441

#include <driver/i2s.h>

#define I2S_PORT        I2S_NUM_0
#define I2S_PIN_BCLK    1     // D0
#define I2S_PIN_WS      2     // D1
#define I2S_PIN_DATA    3     // D2
#define I2S_SAMPLE_RATE 16000
#define I2S_READ_LEN    512

static bool i2sOk = false;

static void i2sInit()
{
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = I2S_SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = 256,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_PIN_BCLK,
    .ws_io_num    = I2S_PIN_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_PIN_DATA
  };
  if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("INMP441 : driver I2S non installe"); return;
  }
  if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
    Serial.println("INMP441 : configuration des pins echouee"); return;
  }
  i2sOk = true;
  Serial.println("INMP441 (I2S) OK");
}

float readSoundDb()
{
  if (!i2sOk) return -1.0f;
  int32_t samples[I2S_READ_LEN];
  size_t  bytesRead = 0;
  i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(100));
  int count = (int)(bytesRead / sizeof(int32_t));
  if (count == 0) return -1.0f;
  double sumSq = 0.0;
  for (int i = 0; i < count; i++) {
    float s = (float)(samples[i] >> 8) / 8388607.0f;
    sumSq += (double)s * s;
  }
  double rms = sqrt(sumSq / count);
  if (rms < 1e-10) return 0.0f;
  float db = 20.0f * (float)log10(rms) + 120.0f;
  return (db < 0.0f) ? 0.0f : db;
}

#else

float readSoundDb() { return -1.0f; }

#endif  // USE_INMP441

// ── Init ──────────────────────────────────────────────────────────────────────

void mesuresInit()
{
#ifdef USE_INMP441
  i2sInit();
#else
  Serial.println("INMP441 : desactive (USE_INMP441 non defini)");
#endif
}
