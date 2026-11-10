#include "epd_ssd2683.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <string.h>

namespace {
constexpr gpio_num_t PIN_POWER = GPIO_NUM_6;
constexpr gpio_num_t PIN_SCK = GPIO_NUM_12;
constexpr gpio_num_t PIN_MOSI = GPIO_NUM_13;
constexpr gpio_num_t PIN_CS = GPIO_NUM_11;
constexpr gpio_num_t PIN_DC = GPIO_NUM_10;
constexpr gpio_num_t PIN_RST = GPIO_NUM_9;
constexpr gpio_num_t PIN_BUSY = GPIO_NUM_8;
constexpr uint32_t BUSY_TIMEOUT_MS = 120000;
constexpr int ROW_BYTES = 100;
constexpr int ROWS = 300;

spi_device_handle_t spi = nullptr;

void setPower(int level) {
  gpio_set_level(PIN_POWER, level);
  gpio_set_direction(PIN_POWER, GPIO_MODE_OUTPUT);
  gpio_hold_dis(PIN_POWER);
  gpio_hold_en(PIN_POWER);
}

void waitBusy() {
  uint32_t start = millis();
  while (gpio_get_level(PIN_BUSY) == 0) {
    if (millis() - start >= BUSY_TIMEOUT_MS) {
      Serial.println("EPD busy timeout");
      return;
    }
    delay(50);
  }
}

void tx(const uint8_t* data, int len) {
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 8 * len;
  t.tx_buffer = data;
  esp_err_t err = spi_device_polling_transmit(spi, &t);
  if (err != ESP_OK) Serial.printf("spi transmit failed: %d\n", err);
}

void sendCmd(uint8_t c) {
  gpio_set_level(PIN_DC, 0);
  gpio_set_level(PIN_CS, 0);
  tx(&c, 1);
  gpio_set_level(PIN_CS, 1);
}

void sendData(uint8_t d) {
  gpio_set_level(PIN_DC, 1);
  gpio_set_level(PIN_CS, 0);
  tx(&d, 1);
  gpio_set_level(PIN_CS, 1);
}

void sendBlock(const uint8_t* p, int len) {
  gpio_set_level(PIN_DC, 1);
  gpio_set_level(PIN_CS, 0);
  tx(p, len);
  gpio_set_level(PIN_CS, 1);
}

void initPins() {
  gpio_config_t o = {};
  o.mode = GPIO_MODE_OUTPUT;
  o.pin_bit_mask = (1ULL << PIN_RST) | (1ULL << PIN_DC) | (1ULL << PIN_CS);
  o.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&o);
  gpio_config_t i = {};
  i.mode = GPIO_MODE_INPUT;
  i.pin_bit_mask = 1ULL << PIN_BUSY;
  gpio_config(&i);
  gpio_set_level(PIN_CS, 1);
  gpio_set_level(PIN_RST, 1);
}

void initSpi() {
  spi_bus_config_t bus = {};
  bus.mosi_io_num = PIN_MOSI;
  bus.miso_io_num = -1;
  bus.sclk_io_num = PIN_SCK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = ROW_BYTES * ROWS;
  esp_err_t err = spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) Serial.printf("spi_bus_initialize failed: %d\n", err);
  spi_device_interface_config_t dev = {};
  dev.spics_io_num = -1;
  dev.clock_speed_hz = 10 * 1000 * 1000;
  dev.mode = 0;
  dev.queue_size = 1;
  err = spi_bus_add_device(SPI3_HOST, &dev, &spi);
  if (err != ESP_OK) Serial.printf("spi_bus_add_device failed: %d\n", err);
}

void deinitSpi() {
  if (spi) spi_bus_remove_device(spi);
  spi = nullptr;
  spi_bus_free(SPI3_HOST);
}
}  // namespace

void Epd::show(const uint8_t* buf) {
  setPower(1);
  delay(10);
  initPins();
  initSpi();

  gpio_set_level(PIN_RST, 1);
  delay(10);
  gpio_set_level(PIN_RST, 0);
  delay(20);
  gpio_set_level(PIN_RST, 1);
  delay(10);
  waitBusy();

  sendCmd(0xE9);
  sendData(0x01);

  sendCmd(0x10);
  waitBusy();
  for (int y = 0; y < ROWS; y++) {
    sendBlock(buf + y * ROW_BYTES, ROW_BYTES);
    if ((y & 15) == 15) delay(1);
  }

  sendCmd(0x04);
  waitBusy();
  delay(10);
  sendCmd(0x12);
  sendData(0x00);
  delay(10);
  waitBusy();
  sendCmd(0x02);
  sendData(0x00);
  waitBusy();
  delay(20);
  sendCmd(0x07);
  sendData(0xA5);

  deinitSpi();
  setPower(0);
}
