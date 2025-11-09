#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <ctime> // added for time()/localtime_r()

namespace esphome
{
  namespace sgready_component
  {
    enum class SGReadyMode : int8_t
    {
      UNKNOWN_OPERAION = 0,     // undefined state
      BLOCKED_OPERATION = 1,    // pin_a=1, pin_b=0, Mode 1 – Blocked operation
      NORMAL_OPERATION = 2,     // pin_a=0, pin_b=0, Mode 2 – Normal operation
      ENCOURAGED_OPERATION = 3, // pin_a=0, pin_b=1, Mode 3 – Encouraged operation
      ORDERED_OPERATION = 4,    // pin_a=1, pin_b=1, Mode 4 – Ordered operation
    };
    struct sgready_mode
    {
      SGReadyMode mode;
      const char *description;
      bool pin_a;
      bool pin_b;
    };

    enum class PriceLevel : int8_t
    {
      PRICE_UNKNOWN = 0,
      PRICE_LEVEL_VERY_LOW = 1,
      PRICE_LEVEL_LOW = 2,
      PRICE_LEVEL_NORMAL = 3,
      PRICE_LEVEL_HIGH = 4,
    };

    class SGReadyComponent : public switch_::Switch, public Component
    {
    public:
      void loop() override;
      void update(PriceLevel price_level, float temperature_sensor_value);
      void dump_config() override;
      void setup() override;

      SGReadyMode get_next_mode(PriceLevel price_level, float temperature_sensor_value, unsigned long ms_since_last_change);

      SGReadyMode set_mode(SGReadyMode mode);

      void set_minimum_operating_temperature(float temperature_celsius);
      void set_output_pin(GPIOPin *pin_a, GPIOPin *pin_b)
      {
        this->pin_a_ = pin_a;
        this->pin_b_ = pin_b;
      }

      void set_allow_ordered_mode(switch_::Switch *s);
      void set_allow_encouraged_mode(switch_::Switch *s);
      void set_sgready_enabled(switch_::Switch *s);

      void set_temperature_sensor(esphome::sensor::Sensor *sensor);
      void set_price_level_sensor(esphome::sensor::Sensor *sensor);
      void set_mode_text_sensor(esphome::text_sensor::TextSensor *t);

      void register_switch(switch_::Switch *sw);
      void write_state(bool state) override;
      void set_pin_a_binary(esphome::binary_sensor::BinarySensor *b);
      void set_pin_b_binary(esphome::binary_sensor::BinarySensor *b);

    protected:
      GPIOPin *pin_a_, *pin_b_;
      esphome::binary_sensor::BinarySensor *pin_a_binary_{nullptr};
      esphome::binary_sensor::BinarySensor *pin_b_binary_{nullptr};
      esphome::text_sensor::TextSensor *mode_text_sensor_{nullptr};
      unsigned long last_mode_change_ms_{0};

      float last_temperature_{NAN};
      esphome::sensor::Sensor *temperature_sensor_{nullptr};

      PriceLevel current_price_level_{PriceLevel::PRICE_LEVEL_NORMAL};
      PriceLevel last_price_level_;
      esphome::sensor::Sensor *price_level_sensor_{nullptr};

      esphome::switch_::Switch *allow_ordered_mode_{nullptr};
      esphome::switch_::Switch *allow_encouraged_mode_{nullptr};
      esphome::switch_::Switch *sgready_enabled_{nullptr};
    };

  } // namespace sgready_component
} // namespace esphome

// # https://www.gridx.ai/knowledge/sg-ready
// # AB
// # 10 - Mode 1 – Blocked operation (1:0): The operation for the heat pump is blocked for a maximum of two hours per day.
// # 00 - Mode 2 – Normal operation (0:0): The heat pump runs in energy-efficient normal mode.
// # 01 - Mode 3 – Encouraged operation (0:1): The operation of the heat pump is encouraged to increase electricity consumption for heating and warm water.
// # 11 - Mode 4 – Ordered operation (1:1): The heat pump is ordered to run. This state supports two variants which must be adjustable on the controller for different tariff and utilization models:
// #           i) the heat pump is switched on
// #           ii) the heat pump is switched on AND the warm water temperature is raised
// #
// #
// # The blocking signal (mode 1) is active for at least 10 minutes and can only be reactivated 10 minutes after it was last active.
// # The blocking signal (mode 1) is only applied for a maximum of 2 hours.
// # The blocking signal (mode 1) is switched no more than 3 times a day.
// # As soon as the signal for mode 3 or 4 is set, it remains active for at least 10 minutes and can only be reactivated 10 minutes after it was last active.
