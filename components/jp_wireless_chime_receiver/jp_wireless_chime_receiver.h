#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/api/custom_api_device.h"

namespace esphome {
namespace jp_wireless_chime_receiver {

class JPWirelessChimeReceiver : public Component, public api::CustomAPIDevice {
 public:
  void set_pin(GPIOPin *pin) { this->pin_ = pin; }
  void set_pin_number(uint8_t pin_number) { this->pin_number_ = pin_number; }

  void setup() override;
  void loop() override;

 protected:
  GPIOPin *pin_{nullptr};
  uint8_t pin_number_{0};
};

}  // namespace jp_wireless_chime_receiver
}  // namespace esphome