#pragma once

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

#define M_PI 3.14159265358979323846

#ifndef DEG2RAD
#define DEG2RAD(g) ((g) * M_PI) / 180
#endif

#define SERVO_MIN_PULSEWIDTH_US 500
#define SERVO_MAX_PULSEWIDTH_US 2500
#define SERVO_MIN_DEGREE -90
#define SERVO_MAX_DEGREE 90
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000
#define SERVO_TIMEBASE_PERIOD 20000

typedef struct {
    bool is_attached_;

    unsigned int amplitude_;
    int offset_;
    unsigned int period_;
    double phase0_;

    int pos_;
    int pin_;
    int trim_;
    double phase_;
    double inc_;
    double number_samples_;
    unsigned int sampling_period_;

    long previous_millis_;
    long current_millis_;
    long previous_servo_command_millis_;

    bool stop_;
    bool rev_;
    int diff_limit_;

    ledc_channel_t ledc_channel_;
    ledc_mode_t ledc_speed_mode_;
} oscillator_t;

unsigned long IRAM_ATTR millis(void);

void oscillator_init(oscillator_t *osc, int trim);
void oscillator_attach(oscillator_t *osc, int pin, bool rev);
void oscillator_detach(oscillator_t *osc);
void oscillator_set_a(oscillator_t *osc, unsigned int amplitude);
void oscillator_set_o(oscillator_t *osc, int offset);
void oscillator_set_ph(oscillator_t *osc, double Ph);
void oscillator_set_t(oscillator_t *osc, unsigned int period);
void oscillator_set_trim(oscillator_t *osc, int trim);
void oscillator_set_position(oscillator_t *osc, int position);
void oscillator_stop(oscillator_t *osc);
void oscillator_play(oscillator_t *osc);
void oscillator_reset(oscillator_t *osc);
void oscillator_refresh(oscillator_t *osc);
int oscillator_get_position(oscillator_t *osc);
void oscillator_set_limiter(oscillator_t *osc, int diff_limit);
void oscillator_disable_limiter(oscillator_t *osc);