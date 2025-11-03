#include "esphome/core/log.h"
#include "sgready_component.h"

namespace esphome
{
    namespace sgready_component
    {

        static const char *TAG = "sgready_component.component";
        // internal configuration (constants) and state (internal linkage)
        namespace
        {
            constexpr int kAllowedBlockedTimesToday = 3;
            constexpr int kAllowedBlockedTimeMin = 60;        // minutes
            constexpr int kAllowedBlockedTimeMax = 120;       // minutes
            constexpr int kMinimumTimeBetweenChangesMin = 10; // minutes

            constexpr unsigned long kMinModeChangeMs = static_cast<unsigned long>(kMinimumTimeBetweenChangesMin) * 60UL * 1000UL;
            constexpr unsigned long kMinBlockedModeMs = static_cast<unsigned long>(kAllowedBlockedTimeMin) * 60UL * 1000UL;
            constexpr unsigned long kMaxBlockedModeMs = static_cast<unsigned long>(kAllowedBlockedTimeMax) * 60UL * 1000UL;

            unsigned long last_mode_change_ms = 0;

            int used_blocked_times_today = 0;
            // int used_blocked_time = 0; // minutes

            float minimum_temperature_celsius = 17.0;

            SGReadyMode current_mode = SGReadyMode::MODE_2;

        } // namespace

        void SGReadyComponent::setup()
        {
            ESP_LOGCONFIG(TAG, "Running setup");
            this->pin_a_->setup();
            this->pin_b_->setup();
            this->set_mode(SGReadyMode::MODE_2);
        }

        void SGReadyComponent::loop()
        {
        }

        void SGReadyComponent::update(PriceLevel price_level, float temperature_sensor_value)
        {
            ESP_LOGI(TAG, "SGReadyComponent::update called with price_level=%d, temperature=%.2f", static_cast<int>(price_level), temperature_sensor_value);

            if (!this->pin_a_ || !this->pin_b_)
            {
                ESP_LOGW(TAG, "Output pins not set, cannot update SGReady mode");
                return;
            }

            SGReadyMode next_mode = this->get_next_mode(price_level);

            unsigned long now = millis();

            if (now != 0 && (now - last_mode_change_ms) < kMinModeChangeMs)
            {
                const unsigned long wait_ms = kMinModeChangeMs - (now - last_mode_change_ms);
                ESP_LOGW(TAG, "Mode change to %d suppressed: need to wait %lu ms more", static_cast<int>(next_mode), wait_ms);
                return;
            }

            if (current_mode != next_mode)
            {
                if (current_mode == SGReadyMode::MODE_1 && next_mode != SGReadyMode::MODE_1)
                {
                    if ((now - last_mode_change_ms) < kMinBlockedModeMs)
                    {
                        const unsigned long wait_ms = kMinBlockedModeMs - (now - last_mode_change_ms);
                        ESP_LOGW(TAG, "Mode change from MODE_1 to %d suppressed: need to stay in MODE_1 for %lu ms more", static_cast<int>(next_mode), wait_ms);
                        return;
                    }
                }
                else if (next_mode == SGReadyMode::MODE_1)
                {
                    used_blocked_times_today++;
                }

                this->set_mode(next_mode);
                ESP_LOGI(TAG, "SGReady mode changed from %d to %d", static_cast<int>(current_mode), static_cast<int>(next_mode));
                // record change time
                last_mode_change_ms = now;
            }
            else
            {
                if (current_mode == SGReadyMode::MODE_1 && (now - last_mode_change_ms) >= kMaxBlockedModeMs)
                {
                    ESP_LOGW(TAG, "Maximum blocked mode time exceeded, switching to MODE_2");
                    this->set_mode(SGReadyMode::MODE_2);
                    current_mode = SGReadyMode::MODE_2;
                    last_mode_change_ms = now;
                    return;
                }
                ESP_LOGI(TAG, "SGReady mode remains unchanged at %d", static_cast<int>(current_mode));
            }
            current_mode = next_mode;
        }

        SGReadyMode SGReadyComponent::get_next_mode(PriceLevel price_level)
        {
            switch (price_level)
            {
            case PriceLevel::PRICE_LEVEL_VERY_LOW:
                if (this->allow_ordered_mode_ != nullptr && this->allow_ordered_mode_->state)
                {
                    return SGReadyMode::MODE_4;
                }
                // Fall through to next level if not allowed
            case PriceLevel::PRICE_LEVEL_LOW:
                if (this->allow_encouraged_mode_ != nullptr && this->allow_encouraged_mode_->state)
                {
                    return SGReadyMode::MODE_3;
                }
                // Fall through to next level if not allowed
            case PriceLevel::PRICE_LEVEL_MEDIUM:
                return SGReadyMode::MODE_2;
            case PriceLevel::PRICE_LEVEL_HIGH:
            default:
                if (this->get_can_use_blocked_mode())
                {
                    return SGReadyMode::MODE_1;
                }
                else
                {
                    return SGReadyMode::MODE_2;
                }
            }
        }

        bool SGReadyComponent::get_can_use_blocked_mode()
        {
            return (used_blocked_times_today < kAllowedBlockedTimesToday);
        }

        void SGReadyComponent::set_minimum_operating_temperature(float temperature_celsius)
        {
            minimum_temperature_celsius = temperature_celsius;
        }

        void SGReadyComponent::set_allow_ordered_mode(switch_::Switch *s)
        {
            allow_ordered_mode_ = s;
        }
        void SGReadyComponent::set_allow_encouraged_mode(switch_::Switch *s)
        {
            allow_encouraged_mode_ = s;
        }

        switch_::Switch *SGReadyComponent::get_allow_ordered_mode() const
        {
            return allow_ordered_mode_;
        }
        switch_::Switch *SGReadyComponent::get_allow_encouraged_mode() const
        {
            return allow_encouraged_mode_;
        }

        void SGReadyComponent::set_mode(SGReadyMode mode)
        {
            // unsigned long now = millis();
            // if (now != 0 && (now - last_mode_change_ms) < kMinModeChangeMs)
            // {
            //     const unsigned long wait_ms = kMinModeChangeMs - (now - last_mode_change_ms);
            //     ESP_LOGW(TAG, "Mode change to %d suppressed: need to wait %lu ms more", static_cast<int>(mode), wait_ms);
            //     return;
            // }

            bool new_pin_a = static_cast<bool>(static_cast<int>(mode) >> 1);
            bool new_pin_b = static_cast<bool>(static_cast<int>(mode) & 0b01);
            ESP_LOGI(TAG, "Setting SGReady mode to %d (pin_a=%d, pin_b=%d)", static_cast<int>(mode), new_pin_a, new_pin_b);
            this->pin_a_->digital_write(new_pin_a);
            this->pin_b_->digital_write(new_pin_b);

            // // record change time
            // last_mode_change_ms = now;
        }

        void SGReadyComponent::write_state(bool state)
        {
            this->publish_state(state);
        }

        void SGReadyComponent::register_switch(switch_::Switch *sw)
        {
            if (sw == nullptr)
            {
                ESP_LOGW(TAG, "register_switch called with nullptr");
                return;
            }

            // Place into first free slot
            if (this->allow_ordered_mode_ == nullptr)
            {
                this->set_allow_ordered_mode(sw);
                ESP_LOGI(TAG, "Registered child switch in slot 1: %s", sw->get_name().c_str());
            }
            else if (this->allow_encouraged_mode_ == nullptr)
            {
                this->set_allow_encouraged_mode(sw);
                ESP_LOGI(TAG, "Registered child switch in slot 2: %s", sw->get_name().c_str());
            }
            else
            {
                ESP_LOGW(TAG, "More than 2 child switches configured, ignoring %s", sw->get_name().c_str());
                return;
            }
        }
        void SGReadyComponent::set_temperature_sensor(esphome::sensor::Sensor *sensor)
        {
            this->temperature_sensor_ = sensor;
            if (!sensor)
                return;

            // subscribe to sensor updates; when sensor changes, call update with last known price
            sensor->add_on_state_callback([this](float value)
                                          {
        this->last_temperature_ = value;
        // call update with current stored price and the new temperature
        this->update(this->current_price_level_, this->last_temperature_); });
        }

        void SGReadyComponent::set_price_level_sensor(esphome::sensor::Sensor *sensor)
        {
            this->price_level_sensor_ = sensor;
            if (!sensor)
                return;

            sensor->add_on_state_callback([this](int value)
                                          {
        this->last_price_level_ = static_cast<PriceLevel>(value);
        // call update with current stored price and the new temperature
        this->update(this->last_price_level_, this->last_temperature_); });
        }

        void SGReadyComponent::dump_config()
        {
            ESP_LOGCONFIG(TAG, "SGReady component");
            ESP_LOGCONFIG(TAG, "GPIO component");
            LOG_PIN("SGReady Pin A: ", this->pin_a_);
            LOG_PIN("SGReady Pin B: ", this->pin_b_);

            ESP_LOGCONFIG(TAG, "Switch Component");
            if (this->allow_ordered_mode_)
                ESP_LOGCONFIG(TAG, "  allow_ordered_mode: %s", this->allow_ordered_mode_->get_name().c_str());
            if (this->allow_encouraged_mode_)
                ESP_LOGCONFIG(TAG, "  allow_encouraged_mode: %s", this->allow_encouraged_mode_->get_name().c_str());
        }

    } // namespace sgready_component
} // namespace esphome