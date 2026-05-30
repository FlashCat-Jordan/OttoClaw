#include "oscillator.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "Oscillator";

unsigned long IRAM_ATTR millis(void) {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

static ledc_channel_t next_free_channel = LEDC_CHANNEL_0;

void oscillator_init(oscillator_t *osc, int trim) {
    osc->trim_ = trim;
    osc->diff_limit_ = 0;
    osc->is_attached_ = false;

    osc->sampling_period_ = 30;
    osc->period_ = 2000;
    osc->number_samples_ = osc->period_ / osc->sampling_period_;
    osc->inc_ = 2 * M_PI / osc->number_samples_;

    osc->amplitude_ = 45;
    osc->phase_ = 0;
    osc->phase0_ = 0;
    osc->offset_ = 0;
    osc->stop_ = false;
    osc->rev_ = false;

    osc->pos_ = 90;
    osc->previous_millis_ = 0;
    osc->previous_servo_command_millis_ = 0;
}

void oscillator_attach(oscillator_t *osc, int pin, bool rev) {
    if (osc->is_attached_) {
        oscillator_detach(osc);
    }

    osc->pin_ = pin;
    osc->rev_ = rev;

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    static int last_channel = 0;
    last_channel = (last_channel + 1) % 7 + 1;
    osc->ledc_channel_ = (ledc_channel_t)last_channel;

    ledc_channel_config_t ledc_channel = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = osc->ledc_channel_,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    osc->ledc_speed_mode_ = LEDC_LOW_SPEED_MODE;

    osc->previous_servo_command_millis_ = millis();

    osc->is_attached_ = true;
    ESP_LOGI(TAG, "Oscillator attached to GPIO %d", pin);
}

void oscillator_detach(oscillator_t *osc) {
    if (!osc->is_attached_)
        return;

    ESP_ERROR_CHECK(ledc_stop(osc->ledc_speed_mode_, osc->ledc_channel_, 0));

    osc->is_attached_ = false;
}

void oscillator_set_a(oscillator_t *osc, unsigned int amplitude) {
    osc->amplitude_ = amplitude;
}

void oscillator_set_o(oscillator_t *osc, int offset) {
    osc->offset_ = offset;
}

void oscillator_set_ph(oscillator_t *osc, double Ph) {
    osc->phase0_ = Ph;
}

void oscillator_set_t(oscillator_t *osc, unsigned int period) {
    osc->period_ = period;
    osc->number_samples_ = osc->period_ / osc->sampling_period_;
    osc->inc_ = 2 * M_PI / osc->number_samples_;
}

void oscillator_set_trim(oscillator_t *osc, int trim) {
    osc->trim_ = trim;
}

void oscillator_set_position(oscillator_t *osc, int position) {
    if (!osc->is_attached_)
        return;

    long currentMillis = millis();

    if (osc->diff_limit_ > 0) {
        int limit = 1;
        int diff = (((int)(currentMillis - osc->previous_servo_command_millis_)) * osc->diff_limit_) / 1000;
        if (diff < 1) diff = 1;
        if (abs(position - osc->pos_) > limit) {
            osc->pos_ += position < osc->pos_ ? -limit : limit;
        } else {
            osc->pos_ = position;
        }
    } else {
        osc->pos_ = position;
    }
    osc->previous_servo_command_millis_ = currentMillis;

    int angle = osc->pos_ + osc->trim_;
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    uint32_t duty = (uint32_t)(((angle / 180.0) * 2.0 + 0.5) * 8191 / 20.0);

    ESP_ERROR_CHECK(ledc_set_duty(osc->ledc_speed_mode_, osc->ledc_channel_, duty));
    ESP_ERROR_CHECK(ledc_update_duty(osc->ledc_speed_mode_, osc->ledc_channel_));
}

void oscillator_stop(oscillator_t *osc) {
    osc->stop_ = true;
}

void oscillator_play(oscillator_t *osc) {
    osc->stop_ = false;
}

void oscillator_reset(oscillator_t *osc) {
    osc->phase_ = 0;
}

int oscillator_get_position(oscillator_t *osc) {
    return osc->pos_;
}

void oscillator_refresh(oscillator_t *osc) {
    if (!osc->is_attached_)
        return;

    osc->current_millis_ = millis();

    if (osc->current_millis_ - osc->previous_millis_ > osc->sampling_period_) {
        osc->previous_millis_ = osc->current_millis_;

        if (!osc->stop_) {
            int pos = (int)round(osc->amplitude_ * sin(osc->phase_ + osc->phase0_) + osc->offset_);
            if (osc->rev_)
                pos = -pos;
            oscillator_set_position(osc, pos + 90);
        }

        osc->phase_ = osc->phase_ + osc->inc_;
    }
}

void oscillator_set_limiter(oscillator_t *osc, int diff_limit) {
    osc->diff_limit_ = diff_limit;
}

void oscillator_disable_limiter(oscillator_t *osc) {
    osc->diff_limit_ = 0;
}