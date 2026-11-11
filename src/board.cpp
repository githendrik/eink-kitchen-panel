#include "board.h"

#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_sleep.h>

namespace {
constexpr gpio_num_t PIN_LATCH = GPIO_NUM_17;
constexpr gpio_num_t PIN_EPD_POWER = GPIO_NUM_6;
constexpr gpio_num_t PIN_CONFIRM = GPIO_NUM_0;
constexpr gpio_num_t PIN_DOWN = GPIO_NUM_18;
constexpr gpio_num_t PIN_UP = GPIO_NUM_39;
constexpr gpio_num_t PIN_CHRG = GPIO_NUM_2;
constexpr gpio_num_t PIN_LED = GPIO_NUM_3;
constexpr gpio_num_t PIN_AUDIO_POWER = GPIO_NUM_42;
constexpr gpio_num_t PIN_AMP = GPIO_NUM_46;

void outputLevel(gpio_num_t pin, int level) {
  // Level and direction are configured while the pad is still held, so releasing the hold
  // never exposes a floating or wrong level.
  gpio_set_level(pin, level);
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  gpio_hold_dis(pin);
}

void input(gpio_num_t pin, bool pullup) {
  gpio_set_direction(pin, GPIO_MODE_INPUT);
  if (pullup) gpio_pullup_en(pin);
}
}  // namespace

namespace board {

void begin() {
  outputLevel(PIN_LATCH, 1);
  gpio_hold_en(PIN_LATCH);

  outputLevel(PIN_EPD_POWER, 0);
  outputLevel(PIN_LED, 1);
  outputLevel(PIN_AUDIO_POWER, 0);
  outputLevel(PIN_AMP, 0);

  input(PIN_CONFIRM, true);
  input(PIN_DOWN, true);
  input(PIN_UP, true);
  input(PIN_CHRG, true);
}

Wake wakeCause() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      return Wake::PowerOn;
    case ESP_SLEEP_WAKEUP_TIMER:
      return Wake::Timer;
    case ESP_SLEEP_WAKEUP_EXT1: {
      uint64_t m = esp_sleep_get_ext1_wakeup_status();
      if (m & (1ULL << PIN_CONFIRM)) return Wake::ButtonConfirm;
      if (m & (1ULL << PIN_DOWN)) return Wake::ButtonDown;
      if (m & (1ULL << PIN_CHRG)) return Wake::Charger;
      return Wake::Other;
    }
    default:
      return Wake::Other;
  }
}

int batteryMillivolts() {
  // Arduino-ESP32 2.x / IDF 4.4 has no curve-fitting scheme; use esp_adc_cal (eFuse-based).
  static esp_adc_cal_characteristics_t chars;
  static bool ready = false;
  if (!ready) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_12);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &chars);
    ready = true;
  }
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC1_CHANNEL_3), &chars) * 2;
  }
  return sum / 10;
}

bool charging() { return gpio_get_level(PIN_CHRG) == 0; }
bool upPressed() { return gpio_get_level(PIN_UP) == 0; }

void led(bool on) {
  gpio_hold_dis(PIN_LED);
  gpio_set_level(PIN_LED, on ? 0 : 1);
  gpio_hold_en(PIN_LED);
}

void deepSleep(int64_t seconds, bool wakeOnCharger) {
  gpio_hold_dis(PIN_EPD_POWER);
  gpio_set_level(PIN_EPD_POWER, 0);
  gpio_hold_en(PIN_EPD_POWER);

  gpio_hold_dis(PIN_LED);
  gpio_set_level(PIN_LED, 1);
  gpio_hold_en(PIN_LED);
  gpio_hold_dis(PIN_AUDIO_POWER);
  gpio_set_level(PIN_AUDIO_POWER, 0);
  gpio_hold_en(PIN_AUDIO_POWER);
  gpio_hold_dis(PIN_AMP);
  gpio_set_level(PIN_AMP, 0);
  gpio_hold_en(PIN_AMP);

  gpio_set_level(PIN_LATCH, 1);
  gpio_hold_en(PIN_LATCH);
  gpio_deep_sleep_hold_en();

  uint64_t mask = (1ULL << PIN_CONFIRM) | (1ULL << PIN_DOWN);
  if (wakeOnCharger) mask |= (1ULL << PIN_CHRG);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  rtc_gpio_pullup_en(PIN_CONFIRM);
  rtc_gpio_pulldown_dis(PIN_CONFIRM);
  rtc_gpio_pullup_en(PIN_DOWN);
  rtc_gpio_pulldown_dis(PIN_DOWN);
  if (wakeOnCharger) {
    rtc_gpio_pullup_en(PIN_CHRG);
    rtc_gpio_pulldown_dis(PIN_CHRG);
  }

  if (seconds > 0) esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  esp_deep_sleep_start();
}

}  // namespace board
