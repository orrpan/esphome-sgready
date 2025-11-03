#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome
{
  namespace sgready_component
  {

    enum class SGReadyMode : uint8_t
    {
      MODE_1 = 0b10, // pin_a=1, pin_b=0, Mode 1 – Blocked operation
      MODE_2 = 0b00, // pin_a=0, pin_b=0, Mode 2 – Normal operation
      MODE_3 = 0b01, // pin_a=0, pin_b=1, Mode 3 – Encouraged operation
      MODE_4 = 0b11, // pin_a=1, pin_b=1, Mode 4 – Ordered operation
    };

    enum class PriceLevel : int8_t
    {
      PRICE_LEVEL_VERY_LOW = 1, // MODE 4
      PRICE_LEVEL_LOW = 2,      // MODE 3
      PRICE_LEVEL_MEDIUM = 3,   // MODE 2
      PRICE_LEVEL_HIGH = 4,     // MODE 1
    };

    class SGReadyComponent : public switch_::Switch, public Component
    {
    public:
      void loop() override;
      void update(PriceLevel price_level, float temperature_sensor_value);
      void dump_config() override;
      void setup() override;

      SGReadyMode get_next_mode(PriceLevel price_level);
      bool get_can_use_blocked_mode();

      void set_mode(SGReadyMode mode);
      void set_minimum_operating_temperature(float temperature_celsius);
      void set_output_pin(GPIOPin *pin_a, GPIOPin *pin_b)
      {
        this->pin_a_ = pin_a;
        this->pin_b_ = pin_b;
      }

      void set_allow_ordered_mode(switch_::Switch *s);
      void set_allow_encouraged_mode(switch_::Switch *s);

      void set_temperature_sensor(esphome::sensor::Sensor *sensor);
      void set_price_level_sensor(esphome::sensor::Sensor *sensor);

      // void set_price_level(PriceLevel price_level);

      switch_::Switch *get_allow_ordered_mode() const;
      switch_::Switch *get_allow_encouraged_mode() const;

      void register_switch(switch_::Switch *sw);
      void write_state(bool state) override;

    protected:
      GPIOPin *pin_a_, *pin_b_;

      float last_temperature_{NAN};
      esphome::sensor::Sensor *temperature_sensor_{nullptr};

      PriceLevel current_price_level_{PriceLevel::PRICE_LEVEL_MEDIUM};
      PriceLevel last_price_level_;
      esphome::sensor::Sensor *price_level_sensor_{nullptr};

      switch_::Switch *allow_ordered_mode_ = nullptr, *allow_encouraged_mode_ = nullptr;
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
