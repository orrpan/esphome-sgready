#include "esphome/core/log.h"
#include "sgready_component.h"
#include <ctime>

namespace esphome
{
    namespace sgready_component
    {

        static const char *TAG = "sgready_component.component";

        // compact configuration/constants in one anonymous namespace
        namespace
        {
            constexpr int kAllowedBlockedTimesToday = 3;
            constexpr int kAllowedBlockedTimeMin = 60;                       // minutes
            constexpr int kAllowedBlockedTimeMax = 120;                      // minutes
            constexpr unsigned long kMinModeChangeMs = 10UL * 60UL * 1000UL; // 10 minutes
            constexpr unsigned long kMaxBlockedModeMs = static_cast<unsigned long>(kAllowedBlockedTimeMax) * 60UL * 1000UL;
            constexpr unsigned long kMinBlockedModeMs = static_cast<unsigned long>(kAllowedBlockedTimeMin) * 60UL * 1000UL;

            // runtime state that is internal to this translation unit
            static unsigned long last_mode_change_ms = 0;
            static int used_blocked_times_today = 0;
            static int last_midnight_day = -1;
            static int last_triggered_slot = -1; // hour*60 + minute

            float minimum_temperature_celsius = 17.0;
            SGReadyMode current_mode = SGReadyMode::NORMAL_OPERATION;

            sgready_mode mode_data[] = {
                {SGReadyMode::UNKNOWN_OPERAION, "Unknown operation", 0, 0},
                {SGReadyMode::BLOCKED_OPERATION, "Blocked operation", 1, 0},
                {SGReadyMode::NORMAL_OPERATION, "Normal operation", 0, 0},
                {SGReadyMode::ENCOURAGED_OPERATION, "Encouraged operation", 0, 1},
                {SGReadyMode::ORDERED_OPERATION, "Ordered operation", 1, 1},
            };

            static const char *mode_description(SGReadyMode mode)
            {
                const sgready_mode *md = &mode_data[static_cast<int>(mode)];
                return md->description;
            }
        }

        // ---------- lifecycle ----------
        void SGReadyComponent::setup()
        {
            ESP_LOGCONFIG(TAG, "SGReady setup");
            last_mode_change_ms = millis();
            // this->sgready_enabled_->state = true; // default to enabled
            if (this->pin_a_)
                this->pin_a_->setup();
            if (this->pin_b_)
                this->pin_b_->setup();

            // ensure pins reflect initial mode
            this->set_mode(current_mode);

            if (this->pin_a_binary_ && this->pin_a_)
                this->pin_a_binary_->publish_state(static_cast<bool>(this->pin_a_->digital_read()));
            if (this->pin_b_binary_ && this->pin_b_)
                this->pin_b_binary_->publish_state(static_cast<bool>(this->pin_b_->digital_read()));

            if (this->sgready_enabled_)
            {
                this->sgready_enabled_->publish_state(true);
            }
        }

        // run periodic tasks: midnight reset + scheduled updates
        void SGReadyComponent::loop()
        {
            // midnight reset (requires time sync)
            time_t now = time(nullptr);
            if (now != (time_t)0)
            {
                struct tm local_tm;
#if defined(_POSIX_THREAD_SAFE_FUNCTIONS)
                localtime_r(&now, &local_tm);
#else
                struct tm *tmp = localtime(&now);
                if (tmp)
                    local_tm = *tmp;
                else
                    return;
#endif
                int today = local_tm.tm_mday;
                if (last_midnight_day == -1)
                    last_midnight_day = today;
                if (today != last_midnight_day)
                {
                    ESP_LOGI(TAG, "New day: resetting used_blocked_times_today (was %d)", used_blocked_times_today);
                    used_blocked_times_today = 0;
                    last_midnight_day = today;
                }

                int minute = local_tm.tm_min;
                int second = local_tm.tm_sec;
                int slot = local_tm.tm_hour * 60 + minute;
                // trigger at minutes 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55 at exactly second 5
                if (minute % 5 == 0 && second == 5 && slot != last_triggered_slot)
                {
                    last_triggered_slot = slot;
                    ESP_LOGI(TAG, "Scheduled tick %02d:%02d -> update()", local_tm.tm_hour, minute);
                    this->update(this->current_price_level_, this->last_temperature_);
                }

                return;
            }
            this->update(PriceLevel::PRICE_LEVEL_NORMAL, 30.0);
        }

        // ---------- core logic ----------
        void SGReadyComponent::update(PriceLevel price_level, float temperature_sensor_value)
        {
            if (this->sgready_enabled_ && !this->sgready_enabled_->state)
            {
                ESP_LOGI(TAG, "SGReady disabled via control switch; setting NORMAL_OPERATION");
                current_mode = this->set_mode(SGReadyMode::NORMAL_OPERATION);
                return;
            }
            ESP_LOGD(TAG, "update(price=%d, temp=%.2f)", static_cast<int>(price_level), temperature_sensor_value);

            if (!this->pin_a_ || !this->pin_b_)
            {
                ESP_LOGW(TAG, "Pins not configured; skipping update");
                return;
            }
            unsigned long now = millis();
            unsigned long ms_since_last_change = now - last_mode_change_ms;

            SGReadyMode next_mode = this->get_next_mode(price_level);

            // prevent rapid mode toggles : enforce minimum time between changes
            if (now != 0 && ms_since_last_change < (kMinModeChangeMs - 1000UL))
            {
                ESP_LOGW(TAG, "Mode change to %s suppressed: wait %lu ms more", mode_description(next_mode), kMinModeChangeMs - ms_since_last_change);
                return;
            }

            // track blocked usage counters
            if (current_mode == SGReadyMode::BLOCKED_OPERATION && next_mode != SGReadyMode::BLOCKED_OPERATION)
            {
                if (ms_since_last_change >= kMinBlockedModeMs)
                {
                    used_blocked_times_today++;
                    ESP_LOGI(TAG, "Blocked session ended and lasted %lu ms, blocked %d times today", ms_since_last_change, used_blocked_times_today);
                }
                else
                {
                    ESP_LOGW(TAG, "Blocked session too short (%lu ms); not counting towards daily limit", ms_since_last_change);
                    // do not count this towards the daily limit
                    next_mode = SGReadyMode::BLOCKED_OPERATION;
                }
            }
            else if (current_mode == SGReadyMode::BLOCKED_OPERATION && next_mode == SGReadyMode::BLOCKED_OPERATION)
            {
                if (ms_since_last_change >= kMaxBlockedModeMs)
                {
                    used_blocked_times_today++;
                    ESP_LOGI(TAG, "Exiting BLOCKED_OPERATION mode due to max duration reached");
                    next_mode = SGReadyMode::NORMAL_OPERATION;
                }
            }
            else if (next_mode == current_mode)
            {
                ESP_LOGD(TAG, "Mode unchanged (%s)", mode_description(current_mode));
                return;
            }

            ESP_LOGI(TAG, "Changing mode %s -> %s", mode_description(current_mode), mode_description(next_mode));
            current_mode = this->set_mode(next_mode);
            last_mode_change_ms = now;
        }

        // Determine next mode based on price level and availability
        SGReadyMode SGReadyComponent::get_next_mode(PriceLevel price_level)
        {
            switch (price_level)
            {
            case PriceLevel::PRICE_LEVEL_VERY_LOW:
                if (this->allow_ordered_mode_ && this->allow_ordered_mode_->state)
                    return SGReadyMode::ORDERED_OPERATION;
                // fallthrough
            case PriceLevel::PRICE_LEVEL_LOW:
                if (this->allow_encouraged_mode_ && this->allow_encouraged_mode_->state)
                    return SGReadyMode::ENCOURAGED_OPERATION;
                // fallthrough
            case PriceLevel::PRICE_LEVEL_NORMAL:
                return SGReadyMode::NORMAL_OPERATION;
            case PriceLevel::PRICE_LEVEL_HIGH:
                if ((used_blocked_times_today < kAllowedBlockedTimesToday) &&
                    (last_temperature_ >= minimum_temperature_celsius))
                    return SGReadyMode::BLOCKED_OPERATION;
                else
                    return SGReadyMode::NORMAL_OPERATION;
            default:
                ESP_LOGW(TAG, "Unknown price level %d; defaulting to NORMAL_OPERATION", static_cast<int>(price_level));
                return SGReadyMode::NORMAL_OPERATION;
            }
        }

        // ---------- setters / helpers ----------
        SGReadyMode SGReadyComponent::set_mode(SGReadyMode mode)
        {
            const sgready_mode *md = &mode_data[static_cast<int>(mode)];

            ESP_LOGI(TAG, "set_mode %s (a=%d b=%d)", md->description, md->pin_a, md->pin_b);

            if (this->pin_a_)
                this->pin_a_->digital_write(!md->pin_a);
            if (this->pin_b_)
                this->pin_b_->digital_write(!md->pin_b);

            if (this->pin_a_binary_)
                this->pin_a_binary_->publish_state(md->pin_a);
            if (this->pin_b_binary_)
                this->pin_b_binary_->publish_state(md->pin_b);
            if (this->mode_text_sensor_)
                this->mode_text_sensor_->publish_state(std::string(md->description));

            return mode;
        }

        void SGReadyComponent::write_state(bool state)
        {
            this->publish_state(state);
        }

        // register a child switch (first = ordered, second = encouraged)
        void SGReadyComponent::register_switch(switch_::Switch *sw)
        {
            if (!sw)
                return;
            if (!this->allow_ordered_mode_)
            {
                this->set_allow_ordered_mode(sw);
                ESP_LOGI(TAG, "registered ordered switch %s", sw->get_name().c_str());
                return;
            }
            if (!this->allow_encouraged_mode_)
            {
                this->set_allow_encouraged_mode(sw);
                ESP_LOGI(TAG, "registered encouraged switch %s", sw->get_name().c_str());
                return;
            }
            if (!this->sgready_enabled_)
            {
                this->set_sgready_enabled(sw);
                ESP_LOGI(TAG, "registered sgready_enabled switch %s", sw->get_name().c_str());
                return;
            }
            ESP_LOGW(TAG, "extra child switch ignored: %s", sw->get_name().c_str());
        }

        void SGReadyComponent::set_allow_ordered_mode(switch_::Switch *s) { this->allow_ordered_mode_ = s; }
        void SGReadyComponent::set_allow_encouraged_mode(switch_::Switch *s) { this->allow_encouraged_mode_ = s; }
        void SGReadyComponent::set_sgready_enabled(switch_::Switch *s) { this->sgready_enabled_ = s; }

        void SGReadyComponent::set_pin_a_binary(esphome::binary_sensor::BinarySensor *b)
        {
            this->pin_a_binary_ = b;
            if (b && this->pin_a_)
                b->publish_state(static_cast<bool>(this->pin_a_->digital_read()));
        }
        void SGReadyComponent::set_pin_b_binary(esphome::binary_sensor::BinarySensor *b)
        {
            this->pin_b_binary_ = b;
            if (b && this->pin_b_)
                b->publish_state(static_cast<bool>(this->pin_b_->digital_read()));
        }

        void SGReadyComponent::set_mode_text_sensor(esphome::text_sensor::TextSensor *t)
        {
            this->mode_text_sensor_ = t;
            if (t)
                t->publish_state(std::string(mode_description(current_mode)));
        }

        void SGReadyComponent::set_temperature_sensor(esphome::sensor::Sensor *sensor)
        {
            this->temperature_sensor_ = sensor;
            if (!sensor)
                return;
            sensor->add_on_state_callback([this](float v)
                                          { this->last_temperature_ = v; });
        }

        void SGReadyComponent::set_price_level_sensor(esphome::sensor::Sensor *sensor)
        {
            this->price_level_sensor_ = sensor;
            if (!sensor)
                return;
            sensor->add_on_state_callback([this](float v)
                                          {
        this->last_price_level_ = static_cast<PriceLevel>(static_cast<int>(v));
        this->current_price_level_ = this->last_price_level_;
        this->update(this->current_price_level_, this->last_temperature_); });
        }

        void SGReadyComponent::dump_config()
        {
            ESP_LOGCONFIG(TAG, "SGReady component:");
            LOG_PIN("  Pin A: ", this->pin_a_);
            LOG_PIN("  Pin B: ", this->pin_b_);
            ESP_LOGCONFIG(TAG, "  Mode: %s", mode_description(current_mode));
            ESP_LOGCONFIG(TAG, "  Used blocked times today: %d", used_blocked_times_today);
            if (this->allow_ordered_mode_)
                ESP_LOGCONFIG(TAG, "  Ordered switch: %s", this->allow_ordered_mode_->get_name().c_str());
            if (this->allow_encouraged_mode_)
                ESP_LOGCONFIG(TAG, "  Encouraged switch: %s", this->allow_encouraged_mode_->get_name().c_str());
            if (this->sgready_enabled_)
                ESP_LOGCONFIG(TAG, "  SGReady enabled switch: %s", this->sgready_enabled_->get_name().c_str());
        }

    } // namespace sgready_component
} // namespace esphome