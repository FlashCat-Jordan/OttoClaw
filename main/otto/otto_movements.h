#ifndef __OTTO_MOVEMENTS_H__
#define __OTTO_MOVEMENTS_H__

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oscillator.h"

#define FORWARD 1
#define BACKWARD -1
#define LEFT 1
#define RIGHT -1
#define BOTH 0
#define SMALL 5
#define MEDIUM 15
#define BIG 30

#define SERVO_LIMIT_DEFAULT 240

#define LEFT_LEG 0
#define RIGHT_LEG 1
#define LEFT_FOOT 2
#define RIGHT_FOOT 3
#define LEFT_HAND 4
#define RIGHT_HAND 5
#define SERVO_COUNT 6

typedef struct {
    oscillator_t servo[SERVO_COUNT];
    int servo_pins[SERVO_COUNT];
    int servo_trim[SERVO_COUNT];
    unsigned long final_time;
    unsigned long partial_time;
    float increment[SERVO_COUNT];
    bool is_otto_resting;
    bool has_hands;
    TaskHandle_t action_task;
} otto_t;

void otto_init(otto_t *otto, int left_leg, int right_leg, int left_foot, int right_foot, int left_hand, int right_hand);
void otto_attach_servos(otto_t *otto);
void otto_detach_servos(otto_t *otto);
void otto_set_trims(otto_t *otto, int left_leg, int right_leg, int left_foot, int right_foot, int left_hand, int right_hand);
void otto_move_servos(otto_t *otto, int time, int servo_target[]);
void otto_home(otto_t *otto, bool hands_down);
bool otto_get_rest_state(otto_t *otto);
void otto_set_rest_state(otto_t *otto, bool state);

void otto_jump(otto_t *otto, float steps, int period);
void otto_walk(otto_t *otto, float steps, int period, int dir, int amount);
void otto_turn(otto_t *otto, float steps, int period, int dir, int amount);
void otto_bend(otto_t *otto, int steps, int period, int dir);
void otto_shake_leg(otto_t *otto, int steps, int period, int dir);
void otto_sit(otto_t *otto);
void otto_updown(otto_t *otto, float steps, int period, int height);
void otto_swing(otto_t *otto, float steps, int period, int height);
void otto_tiptoe_swing(otto_t *otto, float steps, int period, int height);
void otto_jitter(otto_t *otto, float steps, int period, int height);
void otto_ascending_turn(otto_t *otto, float steps, int period, int height);
void otto_moonwalker(otto_t *otto, float steps, int period, int height, int dir);
void otto_crusaito(otto_t *otto, float steps, int period, int height, int dir);
void otto_flapping(otto_t *otto, float steps, int period, int height, int dir);
void otto_whirlwind_leg(otto_t *otto, float steps, int period, int amplitude);
void otto_hands_up(otto_t *otto, int period, int dir);
void otto_hands_down(otto_t *otto, int period, int dir);
void otto_hand_wave(otto_t *otto, int dir);
void otto_windmill(otto_t *otto, float steps, int period, int amplitude);
void otto_takeoff(otto_t *otto, float steps, int period, int amplitude);
void otto_fitness(otto_t *otto, float steps, int period, int amplitude);
void otto_greeting(otto_t *otto, int dir, float steps);
void otto_shy(otto_t *otto, int dir, float steps);
void otto_radio_calisthenics(otto_t *otto);
void otto_magic_circle(otto_t *otto);
void otto_showcase(otto_t *otto);

#endif