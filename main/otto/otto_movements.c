#include "otto_movements.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "OttoMovements";

#define HAND_HOME_POSITION 45

void otto_init(otto_t *otto, int left_leg, int right_leg, int left_foot, int right_foot, int left_hand, int right_hand) {
    memset(otto, 0, sizeof(otto_t));

    otto->servo_pins[LEFT_LEG] = left_leg;
    otto->servo_pins[RIGHT_LEG] = right_leg;
    otto->servo_pins[LEFT_FOOT] = left_foot;
    otto->servo_pins[RIGHT_FOOT] = right_foot;
    otto->servo_pins[LEFT_HAND] = left_hand;
    otto->servo_pins[RIGHT_HAND] = right_hand;

    otto->has_hands = (left_hand != -1 && right_hand != -1);

    for (int i = 0; i < SERVO_COUNT; i++) {
        oscillator_init(&otto->servo[i], 0);
    }

    otto_attach_servos(otto);
    otto->is_otto_resting = false;
    ESP_LOGI(TAG, "Otto initialized: LL=%d, RL=%d, LF=%d, RF=%d, LH=%d, RH=%d",
             left_leg, right_leg, left_foot, right_foot, left_hand, right_hand);
    ESP_LOGI(TAG, "Otto has hands: %s", otto->has_hands ? "YES" : "NO");
}

void otto_attach_servos(otto_t *otto) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (otto->servo_pins[i] != -1) {
            oscillator_attach(&otto->servo[i], otto->servo_pins[i], false);
        }
    }
}

void otto_detach_servos(otto_t *otto) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (otto->servo_pins[i] != -1) {
            oscillator_detach(&otto->servo[i]);
        }
    }
}

void otto_set_trims(otto_t *otto, int left_leg, int right_leg, int left_foot, int right_foot, int left_hand, int right_hand) {
    otto->servo_trim[LEFT_LEG] = left_leg;
    otto->servo_trim[RIGHT_LEG] = right_leg;
    otto->servo_trim[LEFT_FOOT] = left_foot;
    otto->servo_trim[RIGHT_FOOT] = right_foot;

    if (otto->has_hands) {
        otto->servo_trim[LEFT_HAND] = left_hand;
        otto->servo_trim[RIGHT_HAND] = right_hand;
    }

    for (int i = 0; i < SERVO_COUNT; i++) {
        if (otto->servo_pins[i] != -1) {
            oscillator_set_trim(&otto->servo[i], otto->servo_trim[i]);
        }
    }
}

static void otto_write(otto_t *otto, int position) {
    if (otto->servo_pins[LEFT_FOOT] != -1) {
        oscillator_set_position(&otto->servo[LEFT_FOOT], position);
    }
    if (otto->servo_pins[RIGHT_FOOT] != -1) {
        oscillator_set_position(&otto->servo[RIGHT_FOOT], position);
    }
}

void otto_move_servos(otto_t *otto, int time, int servo_target[]) {
    if (otto_get_rest_state(otto) == true) {
        otto_set_rest_state(otto, false);
    }

    otto->final_time = millis() + time;

    if (time > 10) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (otto->servo_pins[i] != -1) {
                otto->increment[i] = (servo_target[i] - oscillator_get_position(&otto->servo[i])) / (time / 10.0);
            }
        }

        int previous_servo_time = millis();

        for (int i = 0; i < SERVO_COUNT; i++) {
            otto->partial_time = (long)(otto->increment[i] * 10.0);
        }

        while (millis() < otto->final_time) {
            for (int i = 0; i < SERVO_COUNT; i++) {
                if (otto->servo_pins[i] != -1) {
                    long current_servo_time = millis();
                    if (current_servo_time - previous_servo_time >= 10) {
                        int pos = oscillator_get_position(&otto->servo[i]) + (int)otto->partial_time;
                        oscillator_set_position(&otto->servo[i], pos);
                        previous_servo_time = current_servo_time;
                    }
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    } else {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (otto->servo_pins[i] != -1) {
                oscillator_set_position(&otto->servo[i], servo_target[i]);
            }
        }
    }

    for (int i = 0; i < SERVO_COUNT; i++) {
        if (otto->servo_pins[i] != -1) {
            oscillator_set_position(&otto->servo[i], servo_target[i]);
        }
    }
}

void otto_home(otto_t *otto, bool hands_down) {
    int home_pos[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
    if (!hands_down && otto->has_hands) {
        home_pos[LEFT_HAND] = HAND_HOME_POSITION;
        home_pos[RIGHT_HAND] = HAND_HOME_POSITION;
    }
    otto_move_servos(otto, 700, home_pos);
    otto_set_rest_state(otto, true);
}

bool otto_get_rest_state(otto_t *otto) {
    return otto->is_otto_resting;
}

void otto_set_rest_state(otto_t *otto, bool state) {
    otto->is_otto_resting = state;
}

void otto_jump(otto_t *otto, float steps, int period) {
    int up[SERVO_COUNT] = {90, 90, 90 - 35, 90 + 35, 90, 90};
    int down[SERVO_COUNT] = {90, 90, 90 - 25, 90 + 25, 90, 90};

    for (int i = 0; i < steps; i++) {
        otto_move_servos(otto, period / 2, up);
        otto_move_servos(otto, period / 2, down);
    }
}

void otto_walk(otto_t *otto, float steps, int period, int dir, int amount) {
    int _A = 25;
    int _O = 0;
    int phase_ = 0;

    if (amount != 0) {
        _A = amount;
    }

    double DI = ((dir == FORWARD) ? 1.0 : -1.0);

    int ALPHA = _A;
    int OL = _O;
    int OR = _O;

    int T = 450;
    int TR = 800;
    int TL = 800;

    if (period <= 1000) {
        T = period / 2;
        TR = period;
        TL = period;
    } else {
        T = period;
        TR = period / 2;
        TL = period / 2;
    }

    for (int x = 0; x < steps; x++) {
        int move_a[SERVO_COUNT] = {90 + OL, 90 + OR, 90 - ALPHA, 90 - ALPHA, 90, 90};
        int move_b[SERVO_COUNT] = {90 + OL, 90 + OR, 90, 90, 90, 90};
        int move_c[SERVO_COUNT] = {90 + OL, 90 + OR, 90 + ALPHA, 90 + ALPHA, 90, 90};
        int move_d[SERVO_COUNT] = {90 + OL, 90 + OR, 90, 90, 90, 90};

        if (dir == FORWARD) {
            move_a[LEFT_LEG] = 90 - OL;
            move_b[LEFT_LEG] = 90 - OL;
            move_c[LEFT_LEG] = 90 - OL;
            move_d[LEFT_LEG] = 90 - OL;
        } else {
            move_a[RIGHT_LEG] = 90 - OR;
            move_b[RIGHT_LEG] = 90 - OR;
            move_c[RIGHT_LEG] = 90 - OR;
            move_d[RIGHT_LEG] = 90 - OR;
        }

        otto_move_servos(otto, TL, move_a);
        otto_move_servos(otto, T, move_b);
        otto_move_servos(otto, TR, move_c);
        otto_move_servos(otto, T, move_d);
    }
}

void otto_turn(otto_t *otto, float steps, int period, int dir, int amount) {
    int _A = 25;
    if (amount != 0) {
        _A = amount;
    }

    int T = period;
    int TR = period;
    int TL = period;

    for (int x = 0; x < steps; x++) {
        int move_1[SERVO_COUNT] = {90, 90, 90 - _A, 90 - _A, 90, 90};
        int move_2[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        int move_3[SERVO_COUNT] = {90, 90, 90 + _A, 90 + _A, 90, 90};
        int move_4[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};

        if (dir == LEFT) {
            move_1[RIGHT_LEG] = 90 + 25;
            move_2[RIGHT_LEG] = 90 + 25;
            move_3[RIGHT_LEG] = 90 + 25;
            move_4[RIGHT_LEG] = 90 + 25;
        } else {
            move_1[LEFT_LEG] = 90 + 25;
            move_2[LEFT_LEG] = 90 + 25;
            move_3[LEFT_LEG] = 90 + 25;
            move_4[LEFT_LEG] = 90 + 25;
        }

        otto_move_servos(otto, TL, move_1);
        otto_move_servos(otto, T, move_2);
        otto_move_servos(otto, TR, move_3);
        otto_move_servos(otto, T, move_4);
    }
}

void otto_bend(otto_t *otto, int steps, int period, int dir) {
    for (int i = 0; i < steps; i++) {
        if (dir == LEFT) {
            int bend_a[SERVO_COUNT] = {90, 90, 90 - 30, 90 - 30, 90, 90};
            int bend_b[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
            otto_move_servos(otto, period, bend_a);
            otto_move_servos(otto, period, bend_b);
        } else {
            int bend_a[SERVO_COUNT] = {90, 90, 90 + 30, 90 + 30, 90, 90};
            int bend_b[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
            otto_move_servos(otto, period, bend_a);
            otto_move_servos(otto, period, bend_b);
        }
    }
}

void otto_shake_leg(otto_t *otto, int steps, int period, int dir) {
    for (int i = 0; i < steps; i++) {
        if (dir == RIGHT) {
            int shake_a[SERVO_COUNT] = {90, 90, 90, 90 - 40, 90, 90};
            int shake_b[SERVO_COUNT] = {90, 90, 90, 90 + 20, 90, 90};
            otto_move_servos(otto, period, shake_a);
            otto_move_servos(otto, period, shake_b);
        } else {
            int shake_a[SERVO_COUNT] = {90, 90, 90 - 40, 90, 90, 90};
            int shake_b[SERVO_COUNT] = {90, 90, 90 + 20, 90, 90, 90};
            otto_move_servos(otto, period, shake_a);
            otto_move_servos(otto, period, shake_b);
        }
    }
}

void otto_sit(otto_t *otto) {
    int sit_pos[SERVO_COUNT] = {90, 90, 150, 30, 90, 90};
    otto_move_servos(otto, 1000, sit_pos);
    otto_set_rest_state(otto, true);
}

void otto_updown(otto_t *otto, float steps, int period, int height) {
    int up[SERVO_COUNT] = {90, 90, 90 - height, 90 + height, 90, 90};
    int down[SERVO_COUNT] = {90, 90, 90 + 10, 90 - 10, 90, 90};
    for (int i = 0; i < steps; i++) {
        otto_move_servos(otto, period, up);
        otto_move_servos(otto, period, down);
    }
}

void otto_swing(otto_t *otto, float steps, int period, int height) {
    int swing1[SERVO_COUNT] = {90, 90, 90, 90 + height, 90, 90};
    int swing2[SERVO_COUNT] = {90, 90, 90, 90 - height, 90, 90};
    for (int i = 0; i < steps; i++) {
        otto_move_servos(otto, period, swing1);
        otto_move_servos(otto, period, swing2);
    }
}

void otto_tiptoe_swing(otto_t *otto, float steps, int period, int height) {
    int swing1[SERVO_COUNT] = {90 - height, 90 + height, 90, 90 + height, 90, 90};
    int swing2[SERVO_COUNT] = {90 + height, 90 - height, 90, 90 - height, 90, 90};
    for (int i = 0; i < steps; i++) {
        otto_move_servos(otto, period, swing1);
        otto_move_servos(otto, period, swing2);
    }
}

void otto_jitter(otto_t *otto, float steps, int period, int height) {
    int jitter_up[SERVO_COUNT] = {90 - height, 90 + height, 90 - height, 90 + height, 90, 90};
    int jitter_down[SERVO_COUNT] = {90 + height, 90 - height, 90 + height, 90 - height, 90, 90};
    for (int i = 0; i < steps; i++) {
        otto_move_servos(otto, period, jitter_up);
        otto_move_servos(otto, period, jitter_down);
    }
}

void otto_ascending_turn(otto_t *otto, float steps, int period, int height) {
    for (int i = 0; i < steps; i++) {
        int ascend[SERVO_COUNT] = {90 - height, 90 + height, 90 - height, 90 + height, 90, 90};
        int descend[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        otto_move_servos(otto, period, ascend);
        otto_move_servos(otto, period, descend);
    }
}

void otto_moonwalker(otto_t *otto, float steps, int period, int height, int dir) {
    int move_1[SERVO_COUNT] = {90, 90, 90, 90 + height, 90, 90};
    int move_2[SERVO_COUNT] = {90, 90, 90 - 20, 90 + height, 90, 90};
    int move_3[SERVO_COUNT] = {90, 90, 90 - 20, 90, 90, 90};
    int move_4[SERVO_COUNT] = {90, 90, 90 - 20, 90 - height, 90, 90};
    int move_5[SERVO_COUNT] = {90, 90, 90, 90 - height, 90, 90};
    int move_6[SERVO_COUNT] = {90, 90, 90 + 20, 90 - height, 90, 90};
    int move_7[SERVO_COUNT] = {90, 90, 90 + 20, 90, 90, 90};
    int move_8[SERVO_COUNT] = {90, 90, 90 + 20, 90 + height, 90, 90};

    for (int i = 0; i < steps; i++) {
        if (dir == LEFT) {
            otto_move_servos(otto, period, move_1);
            otto_move_servos(otto, period, move_2);
            otto_move_servos(otto, period, move_3);
            otto_move_servos(otto, period, move_4);
            otto_move_servos(otto, period, move_5);
            otto_move_servos(otto, period, move_6);
            otto_move_servos(otto, period, move_7);
            otto_move_servos(otto, period, move_8);
        } else {
            otto_move_servos(otto, period, move_8);
            otto_move_servos(otto, period, move_7);
            otto_move_servos(otto, period, move_6);
            otto_move_servos(otto, period, move_5);
            otto_move_servos(otto, period, move_4);
            otto_move_servos(otto, period, move_3);
            otto_move_servos(otto, period, move_2);
            otto_move_servos(otto, period, move_1);
        }
    }
}

void otto_crusaito(otto_t *otto, float steps, int period, int height, int dir) {
    for (int i = 0; i < steps; i++) {
        int move_up[SERVO_COUNT] = {90, 90 + 20, 90 - height, 90 - height, 90, 90};
        int move_down[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        int move_forward[SERVO_COUNT] = {90, 90 + 20, 90 - height, 90 - height, 90 + 20, 90 + 20};

        if (dir == FORWARD) {
            otto_move_servos(otto, period / 2, move_up);
            otto_move_servos(otto, period / 2, move_down);
            otto_move_servos(otto, period / 2, move_forward);
            otto_move_servos(otto, period / 2, move_down);
        } else {
            otto_move_servos(otto, period / 2, move_up);
            otto_move_servos(otto, period / 2, move_down);
            move_forward[LEFT_LEG] = 90;
            move_forward[RIGHT_LEG] = 90 - 20;
            otto_move_servos(otto, period / 2, move_forward);
            otto_move_servos(otto, period / 2, move_down);
        }
    }
}

void otto_flapping(otto_t *otto, float steps, int period, int height, int dir) {
    for (int i = 0; i < steps; i++) {
        int flap1[SERVO_COUNT] = {90 - height, 90 + height, 90, 90, 90, 90};
        int flap2[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        int flap3[SERVO_COUNT] = {90 + height, 90 - height, 90, 90, 90, 90};

        if (dir == FORWARD) {
            otto_move_servos(otto, period, flap1);
            otto_move_servos(otto, period, flap2);
            otto_move_servos(otto, period, flap3);
            otto_move_servos(otto, period, flap2);
        } else {
            otto_move_servos(otto, period, flap3);
            otto_move_servos(otto, period, flap2);
            otto_move_servos(otto, period, flap1);
            otto_move_servos(otto, period, flap2);
        }
    }
}

void otto_whirlwind_leg(otto_t *otto, float steps, int period, int amplitude) {
    for (int i = 0; i < steps; i++) {
        for (int j = 0; j < 8; j++) {
            int angle = (j * 180) / 8;
            int whirl[SERVO_COUNT] = {90 + amplitude, 90 + amplitude, 90 + amplitude, 90 + amplitude, 90, 90};
            otto_move_servos(otto, period / 8, whirl);
        }
    }
}

void otto_hands_up(otto_t *otto, int period, int dir) {
    if (!otto->has_hands) {
        ESP_LOGW(TAG, "Otto has no hands!");
        return;
    }

    if (dir == 0) {
        int up_both[SERVO_COUNT] = {90, 90, 90, 90, 180, 180};
        otto_move_servos(otto, period, up_both);
    } else if (dir > 0) {
        int up_left[SERVO_COUNT] = {90, 90, 90, 90, 180, 90};
        otto_move_servos(otto, period, up_left);
    } else {
        int up_right[SERVO_COUNT] = {90, 90, 90, 90, 90, 180};
        otto_move_servos(otto, period, up_right);
    }
}

void otto_hands_down(otto_t *otto, int period, int dir) {
    if (!otto->has_hands) {
        ESP_LOGW(TAG, "Otto has no hands!");
        return;
    }

    if (dir == 0) {
        int down_both[SERVO_COUNT] = {90, 90, 90, 90, 0, 0};
        otto_move_servos(otto, period, down_both);
    } else if (dir > 0) {
        int down_left[SERVO_COUNT] = {90, 90, 90, 90, 0, 90};
        otto_move_servos(otto, period, down_left);
    } else {
        int down_right[SERVO_COUNT] = {90, 90, 90, 90, 90, 0};
        otto_move_servos(otto, period, down_right);
    }
}

void otto_hand_wave(otto_t *otto, int dir) {
    if (!otto->has_hands) {
        ESP_LOGW(TAG, "Otto has no hands!");
        return;
    }

    for (int i = 0; i < 3; i++) {
        if (dir == LEFT) {
            int wave1[SERVO_COUNT] = {90, 90, 90, 90, 120, 0};
            int wave2[SERVO_COUNT] = {90, 90, 90, 90, 90, 0};
            int wave3[SERVO_COUNT] = {90, 90, 90, 90, 120, 0};
            int wave4[SERVO_COUNT] = {90, 90, 90, 90, 90, 0};
            otto_move_servos(otto, 200, wave1);
            otto_move_servos(otto, 200, wave2);
            otto_move_servos(otto, 200, wave3);
            otto_move_servos(otto, 200, wave4);
        } else {
            int wave1[SERVO_COUNT] = {90, 90, 90, 90, 0, 120};
            int wave2[SERVO_COUNT] = {90, 90, 90, 90, 0, 90};
            int wave3[SERVO_COUNT] = {90, 90, 90, 90, 0, 120};
            int wave4[SERVO_COUNT] = {90, 90, 90, 90, 0, 90};
            otto_move_servos(otto, 200, wave1);
            otto_move_servos(otto, 200, wave2);
            otto_move_servos(otto, 200, wave3);
            otto_move_servos(otto, 200, wave4);
        }
    }
}

void otto_windmill(otto_t *otto, float steps, int period, int amplitude) {
    if (!otto->has_hands) {
        ESP_LOGW(TAG, "Otto has no hands!");
        return;
    }

    for (int i = 0; i < steps; i++) {
        int windmill1[SERVO_COUNT] = {90, 90, 90 - 30, 90 + 30, 90 + amplitude, 90 + amplitude};
        int windmill2[SERVO_COUNT] = {90, 90, 90, 90, 90 + amplitude, 90 + amplitude};
        int windmill3[SERVO_COUNT] = {90, 90, 90 + 30, 90 - 30, 90 + amplitude, 90 + amplitude};
        int windmill4[SERVO_COUNT] = {90, 90, 90, 90, 90 + amplitude, 90 + amplitude};

        otto_move_servos(otto, period, windmill1);
        otto_move_servos(otto, period, windmill2);
        otto_move_servos(otto, period, windmill3);
        otto_move_servos(otto, period, windmill4);
    }
}

void otto_takeoff(otto_t *otto, float steps, int period, int amplitude) {
    if (!otto->has_hands) {
        ESP_LOGW(TAG, "Otto has no hands!");
        return;
    }

    int takeoff1[SERVO_COUNT] = {90, 90, 90, 90, 90 + amplitude, 90 + amplitude};
    int takeoff2[SERVO_COUNT] = {90, 90, 90 - amplitude, 90 + amplitude, 90 + amplitude, 90 + amplitude};
    int takeoff3[SERVO_COUNT] = {90, 90, 90, 90, 90 + amplitude, 90 + amplitude};

    for (int i = 0; i < steps; i++) {
        otto_move_servos(otto, period, takeoff1);
        otto_move_servos(otto, period, takeoff2);
        otto_move_servos(otto, period, takeoff3);
    }
}

void otto_fitness(otto_t *otto, float steps, int period, int amplitude) {
    if (!otto->has_hands) {
        ESP_LOGW(TAG, "Otto has no hands!");
        return;
    }

    for (int i = 0; i < steps; i++) {
        int fit1[SERVO_COUNT] = {90, 90, 90 - 30, 90 + 30, 90, 90};
        int fit2[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        int fit3[SERVO_COUNT] = {90, 90, 90 + 30, 90 - 30, 90, 90};
        int fit4[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        int fit5[SERVO_COUNT] = {90, 90, 90, 90, 90 + amplitude, 90 + amplitude};
        int fit6[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        int fit7[SERVO_COUNT] = {90, 90, 90, 90, 90 - amplitude, 90 - amplitude};
        int fit8[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};

        otto_move_servos(otto, period, fit1);
        otto_move_servos(otto, period, fit2);
        otto_move_servos(otto, period, fit3);
        otto_move_servos(otto, period, fit4);
        otto_move_servos(otto, period, fit5);
        otto_move_servos(otto, period, fit6);
        otto_move_servos(otto, period, fit7);
        otto_move_servos(otto, period, fit8);
    }
}

void otto_greeting(otto_t *otto, int dir, float steps) {
    if (!otto->has_hands) {
        ESP_LOGW(TAG, "Otto has no hands!");
        return;
    }

    for (int i = 0; i < steps; i++) {
        if (dir == LEFT) {
            int greet1[SERVO_COUNT] = {90, 90, 90, 90, 90, 0};
            int greet2[SERVO_COUNT] = {90, 90, 90, 90, 120, 0};
            int greet3[SERVO_COUNT] = {90, 90, 90, 90, 150, 0};
            int greet4[SERVO_COUNT] = {90, 90, 90, 90, 120, 0};

            otto_move_servos(otto, 200, greet1);
            otto_move_servos(otto, 200, greet2);
            otto_move_servos(otto, 200, greet3);
            otto_move_servos(otto, 200, greet4);
        } else {
            int greet1[SERVO_COUNT] = {90, 90, 90, 90, 0, 90};
            int greet2[SERVO_COUNT] = {90, 90, 90, 90, 0, 120};
            int greet3[SERVO_COUNT] = {90, 90, 90, 90, 0, 150};
            int greet4[SERVO_COUNT] = {90, 90, 90, 90, 0, 120};

            otto_move_servos(otto, 200, greet1);
            otto_move_servos(otto, 200, greet2);
            otto_move_servos(otto, 200, greet3);
            otto_move_servos(otto, 200, greet4);
        }
    }
}

void otto_shy(otto_t *otto, int dir, float steps) {
    if (!otto->has_hands) {
        ESP_LOGW(TAG, "Otto has no hands!");
        return;
    }

    for (int i = 0; i < steps; i++) {
        if (dir == LEFT) {
            int shy1[SERVO_COUNT] = {90, 90, 90, 90, 90, 0};
            int shy2[SERVO_COUNT] = {90, 90, 90, 90, 90, 60};
            int shy3[SERVO_COUNT] = {90, 90, 90, 90, 90, 30};
            int shy4[SERVO_COUNT] = {90, 90, 90, 90, 90, 60};
            int shy5[SERVO_COUNT] = {90, 90, 90, 90, 90, 0};

            otto_move_servos(otto, 200, shy1);
            otto_move_servos(otto, 200, shy2);
            otto_move_servos(otto, 200, shy3);
            otto_move_servos(otto, 200, shy4);
            otto_move_servos(otto, 200, shy5);
        } else {
            int shy1[SERVO_COUNT] = {90, 90, 90, 90, 0, 90};
            int shy2[SERVO_COUNT] = {90, 90, 90, 90, 60, 90};
            int shy3[SERVO_COUNT] = {90, 90, 90, 90, 30, 90};
            int shy4[SERVO_COUNT] = {90, 90, 90, 90, 60, 90};
            int shy5[SERVO_COUNT] = {90, 90, 90, 90, 0, 90};

            otto_move_servos(otto, 200, shy1);
            otto_move_servos(otto, 200, shy2);
            otto_move_servos(otto, 200, shy3);
            otto_move_servos(otto, 200, shy4);
            otto_move_servos(otto, 200, shy5);
        }
    }
}

void otto_radio_calisthenics(otto_t *otto) {
    if (!otto->has_hands) {
        ESP_LOGW(TAG, "Otto has no hands!");
        return;
    }

    for (int i = 0; i < 2; i++) {
        int radio1[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        int radio2[SERVO_COUNT] = {90, 90, 90, 90, 180, 180};
        int radio3[SERVO_COUNT] = {90, 90, 90 - 30, 90 + 30, 90, 90};
        int radio4[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        int radio5[SERVO_COUNT] = {90, 90, 90 - 30, 90 + 30, 180, 180};
        int radio6[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        int radio7[SERVO_COUNT] = {90, 90, 90, 90, 0, 0};
        int radio8[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};

        otto_move_servos(otto, 500, radio1);
        otto_move_servos(otto, 500, radio2);
        otto_move_servos(otto, 500, radio3);
        otto_move_servos(otto, 500, radio4);
        otto_move_servos(otto, 500, radio5);
        otto_move_servos(otto, 500, radio6);
        otto_move_servos(otto, 500, radio7);
        otto_move_servos(otto, 500, radio8);
    }
}

void otto_magic_circle(otto_t *otto) {
    if (!otto->has_hands) {
        ESP_LOGW(TAG, "Otto has no hands!");
        otto_turn(otto, 4, 500, LEFT, 25);
        return;
    }

    for (int i = 0; i < 4; i++) {
        int magic1[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        int magic2[SERVO_COUNT] = {90, 90, 90 - 30, 90 + 30, 90, 90};
        int magic3[SERVO_COUNT] = {90, 90, 90 - 30, 90 + 30, 180, 180};
        int magic4[SERVO_COUNT] = {90, 90, 90, 90, 180, 180};
        int magic5[SERVO_COUNT] = {90, 90, 90 - 30, 90 + 30, 180, 180};
        int magic6[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        int magic7[SERVO_COUNT] = {90, 90, 90 - 30, 90 + 30, 0, 0};
        int magic8[SERVO_COUNT] = {90, 90, 90, 90, 0, 0};
        int magic9[SERVO_COUNT] = {90, 90, 90 - 30, 90 + 30, 0, 0};
        int magic10[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};

        otto_move_servos(otto, 200, magic1);
        otto_move_servos(otto, 200, magic2);
        otto_move_servos(otto, 200, magic3);
        otto_move_servos(otto, 200, magic4);
        otto_move_servos(otto, 200, magic5);
        otto_move_servos(otto, 200, magic6);
        otto_move_servos(otto, 200, magic7);
        otto_move_servos(otto, 200, magic8);
        otto_move_servos(otto, 200, magic9);
        otto_move_servos(otto, 200, magic10);
    }
}

void otto_showcase(otto_t *otto) {
    ESP_LOGI(TAG, "Running showcase...");
    otto_home(otto, true);
    vTaskDelay(pdMS_TO_TICKS(500));

    otto_jump(otto, 3, 1000);
    vTaskDelay(pdMS_TO_TICKS(500));

    otto_walk(otto, 4, 1000, FORWARD, 25);
    vTaskDelay(pdMS_TO_TICKS(500));

    otto_turn(otto, 4, 1000, LEFT, 25);
    vTaskDelay(pdMS_TO_TICKS(500));

    otto_home(otto, true);
    ESP_LOGI(TAG, "Showcase complete");
}