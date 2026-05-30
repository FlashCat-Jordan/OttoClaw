#include "otto_movements.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "OttoMovements";

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

void otto_move_servos(otto_t *otto, int time, int servo_target[]) {
    if (otto_get_rest_state(otto) == true) {
        otto_set_rest_state(otto, false);
    }

    unsigned long final_time = millis() + time;
    int start_pos[SERVO_COUNT];

    for (int i = 0; i < SERVO_COUNT; i++) {
        if (otto->servo_pins[i] != -1) {
            start_pos[i] = oscillator_get_position(&otto->servo[i]);
        }
    }

    if (time > 10) {
        while (millis() < final_time) {
            unsigned long progress = millis();
            float ratio = (float)(progress - (final_time - time)) / time;
            if (ratio < 0) ratio = 0;
            if (ratio > 1) ratio = 1;

            for (int i = 0; i < SERVO_COUNT; i++) {
                if (otto->servo_pins[i] != -1) {
                    int pos = start_pos[i] + (int)((servo_target[i] - start_pos[i]) * ratio);
                    oscillator_set_position(&otto->servo[i], pos);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    for (int i = 0; i < SERVO_COUNT; i++) {
        if (otto->servo_pins[i] != -1) {
            oscillator_set_position(&otto->servo[i], servo_target[i]);
        }
    }
}

static void otto_oscillate_servos(otto_t *otto, int amplitude[], int offset[], int period, double phase_diff[], float cycle) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (otto->servo_pins[i] != -1) {
            oscillator_set_a(&otto->servo[i], amplitude[i]);
            oscillator_set_o(&otto->servo[i], offset[i]);
            oscillator_set_t(&otto->servo[i], period);
            oscillator_set_ph(&otto->servo[i], phase_diff[i]);
        }
    }

    unsigned long ref = millis();
    unsigned long end_time = (unsigned long)(period * cycle) + ref;

    while (millis() < end_time) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (otto->servo_pins[i] != -1) {
                oscillator_refresh(&otto->servo[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

static void otto_execute(otto_t *otto, int amplitude[], int offset[], int period, double phase_diff[], float steps) {
    if (otto_get_rest_state(otto) == true) {
        otto_set_rest_state(otto, false);
    }

    int cycles = (int)steps;

    if (cycles >= 1) {
        for (int i = 0; i < cycles; i++) {
            otto_oscillate_servos(otto, amplitude, offset, period, phase_diff, 1.0);
        }
    }

    float remaining = steps - cycles;
    if (remaining > 0) {
        otto_oscillate_servos(otto, amplitude, offset, period, phase_diff, remaining);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

void otto_home(otto_t *otto, bool hands_down) {
    if (otto->is_otto_resting == false) {
        int homes[SERVO_COUNT];
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (i == LEFT_HAND || i == RIGHT_HAND) {
                if (hands_down) {
                    homes[i] = (i == LEFT_HAND) ? HAND_HOME_POSITION : 180 - HAND_HOME_POSITION;
                } else {
                    homes[i] = oscillator_get_position(&otto->servo[i]);
                }
            } else {
                homes[i] = 90;
            }
        }

        otto_move_servos(otto, 700, homes);
        otto->is_otto_resting = true;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
}

bool otto_get_rest_state(otto_t *otto) {
    return otto->is_otto_resting;
}

void otto_set_rest_state(otto_t *otto, bool state) {
    otto->is_otto_resting = state;
}

void otto_jump(otto_t *otto, float steps, int period) {
    int up[SERVO_COUNT] = {90, 90, 150, 30, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int down[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    otto_move_servos(otto, period, up);
    otto_move_servos(otto, period, down);
}

void otto_walk(otto_t *otto, float steps, int period, int dir, int amount) {
    int A[SERVO_COUNT] = {30, 30, 30, 30, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 5, -5, HAND_HOME_POSITION - 90, HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(dir * -90), DEG2RAD(dir * -90), 0, 0};

    if (amount > 0 && otto->has_hands) {
        A[LEFT_HAND] = amount;
        A[RIGHT_HAND] = amount;
        phase_diff[LEFT_HAND] = phase_diff[RIGHT_LEG];
        phase_diff[RIGHT_HAND] = phase_diff[LEFT_LEG];
    } else {
        A[LEFT_HAND] = 0;
        A[RIGHT_HAND] = 0;
    }

    otto_execute(otto, A, O, period, phase_diff, steps);
}

void otto_turn(otto_t *otto, float steps, int period, int dir, int amount) {
    int A[SERVO_COUNT] = {30, 30, 30, 30, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 5, -5, HAND_HOME_POSITION - 90, HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(-90), 0, 0};

    if (dir == LEFT) {
        A[0] = 30;
        A[1] = 0;
    } else {
        A[0] = 0;
        A[1] = 30;
    }

    if (amount > 0 && otto->has_hands) {
        A[LEFT_HAND] = amount;
        A[RIGHT_HAND] = amount;
        phase_diff[LEFT_HAND] = phase_diff[LEFT_LEG];
        phase_diff[RIGHT_HAND] = phase_diff[RIGHT_LEG];
    } else {
        A[LEFT_HAND] = 0;
        A[RIGHT_HAND] = 0;
    }

    otto_execute(otto, A, O, period, phase_diff, steps);
}

void otto_bend(otto_t *otto, int steps, int period, int dir) {
    int bend1[SERVO_COUNT] = {90, 90, 62, 35, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int bend2[SERVO_COUNT] = {90, 90, 62, 105, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int homes[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == -1) {
        bend1[2] = 180 - 35;
        bend1[3] = 180 - 60;
        bend2[2] = 180 - 105;
        bend2[3] = 180 - 60;
    }

    int T2 = 800;

    for (int i = 0; i < steps; i++) {
        otto_move_servos(otto, T2 / 2, bend1);
        otto_move_servos(otto, T2 / 2, bend2);
        vTaskDelay(pdMS_TO_TICKS((int)(period * 0.8)));
        otto_move_servos(otto, 500, homes);
    }
}

void otto_shake_leg(otto_t *otto, int steps, int period, int dir) {
    int numberLegMoves = 2;

    int shake_leg1[SERVO_COUNT] = {90, 90, 58, 35, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int shake_leg2[SERVO_COUNT] = {90, 90, 58, 120, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int shake_leg3[SERVO_COUNT] = {90, 90, 58, 60, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int homes[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == LEFT) {
        shake_leg1[2] = 180 - 35;
        shake_leg1[3] = 180 - 58;
        shake_leg2[2] = 180 - 120;
        shake_leg2[3] = 180 - 58;
        shake_leg3[2] = 180 - 60;
        shake_leg3[3] = 180 - 58;
    }

    int T2 = 1000;
    period = period - T2;
    if (period < 200 * numberLegMoves) {
        period = 200 * numberLegMoves;
    }

    for (int j = 0; j < steps; j++) {
        otto_move_servos(otto, T2 / 2, shake_leg1);
        otto_move_servos(otto, T2 / 2, shake_leg2);

        for (int i = 0; i < numberLegMoves; i++) {
            otto_move_servos(otto, period / (2 * numberLegMoves), shake_leg3);
            otto_move_servos(otto, period / (2 * numberLegMoves), shake_leg2);
        }
        otto_move_servos(otto, 500, homes);
    }

    vTaskDelay(pdMS_TO_TICKS(period));
}

void otto_sit(otto_t *otto) {
    int target[SERVO_COUNT] = {120, 60, 0, 180, 45, 135};
    otto_move_servos(otto, 600, target);
}

void otto_updown(otto_t *otto, float steps, int period, int height) {
    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height, -height, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(90), 0, 0};

    otto_execute(otto, A, O, period, phase_diff, steps);
}

void otto_swing(otto_t *otto, float steps, int period, int height) {
    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height / 2, -height / 2, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};

    otto_execute(otto, A, O, period, phase_diff, steps);
}

void otto_tiptoe_swing(otto_t *otto, float steps, int period, int height) {
    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height, -height, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};

    otto_execute(otto, A, O, period, phase_diff, steps);
}

void otto_jitter(otto_t *otto, float steps, int period, int height) {
    if (height > 25) height = 25;
    int A[SERVO_COUNT] = {height, height, 0, 0, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 0, 0, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(-90), DEG2RAD(90), 0, 0, 0, 0};

    otto_execute(otto, A, O, period, phase_diff, steps);
}

void otto_ascending_turn(otto_t *otto, float steps, int period, int height) {
    if (height > 13) height = 13;
    int A[SERVO_COUNT] = {height, height, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height + 4, -height + 4, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(-90), DEG2RAD(90), DEG2RAD(-90), DEG2RAD(90), 0, 0};

    otto_execute(otto, A, O, period, phase_diff, steps);
}

void otto_moonwalker(otto_t *otto, float steps, int period, int height, int dir) {
    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height / 2 + 2, -height / 2 - 2, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phi = -dir * 90;
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(phi), DEG2RAD(-60 * dir + phi), 0, 0};

    otto_execute(otto, A, O, period, phase_diff, steps);
}

void otto_crusaito(otto_t *otto, float steps, int period, int height, int dir) {
    int A[SERVO_COUNT] = {25, 25, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height / 2 + 4, -height / 2 - 4, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(90), DEG2RAD(90), 0, DEG2RAD(-60 * dir), 0, 0};

    otto_execute(otto, A, O, period, phase_diff, steps);
}

void otto_flapping(otto_t *otto, float steps, int period, int height, int dir) {
    int A[SERVO_COUNT] = {12, 12, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height - 10, -height + 10, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(0), DEG2RAD(180), DEG2RAD(-90 * dir), DEG2RAD(90 * dir), 0, 0};

    otto_execute(otto, A, O, period, phase_diff, steps);
}

void otto_whirlwind_leg(otto_t *otto, float steps, int period, int amplitude) {
    int target[SERVO_COUNT] = {90, 90, 180, 90, HAND_HOME_POSITION, 20};
    otto_move_servos(otto, 100, target);
    target[RIGHT_FOOT] = 160;
    otto_move_servos(otto, 500, target);
    vTaskDelay(pdMS_TO_TICKS(1000));

    for (int i = 0; i < steps; i++) {
        for (int j = 0; j < 8; j++) {
            int angle = 90 + (j * 180) / 8;
            int whirl[SERVO_COUNT] = {angle, angle, 180, 160, HAND_HOME_POSITION, 20};
            otto_move_servos(otto, period / 8, whirl);
        }
    }
}

void otto_hands_up(otto_t *otto, int period, int dir) {
    if (!otto->has_hands) {
        return;
    }

    int target[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == 0) {
        target[LEFT_HAND] = 170;
        target[RIGHT_HAND] = 10;
    } else if (dir > 0) {
        target[LEFT_HAND] = 170;
    } else {
        target[RIGHT_HAND] = 10;
    }

    otto_move_servos(otto, period, target);
}

void otto_hands_down(otto_t *otto, int period, int dir) {
    if (!otto->has_hands) {
        return;
    }

    int target[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    otto_move_servos(otto, period, target);
}

static void otto_hand_wave_internal(otto_t *otto, int dir) {
    if (!otto->has_hands) {
        return;
    }

    if (dir == LEFT) {
        for (int i = 0; i < 5; i++) {
            int wave1[SERVO_COUNT] = {90, 90, 90, 90, 160, 135};
            int wave2[SERVO_COUNT] = {90, 90, 90, 90, 100, 135};
            otto_move_servos(otto, 150, wave1);
            otto_move_servos(otto, 150, wave2);
        }
    } else if (dir == RIGHT) {
        for (int i = 0; i < 5; i++) {
            int wave1[SERVO_COUNT] = {90, 90, 90, 90, 45, 20};
            int wave2[SERVO_COUNT] = {90, 90, 90, 90, 45, 70};
            otto_move_servos(otto, 150, wave1);
            otto_move_servos(otto, 150, wave2);
        }
    } else {
        for (int i = 0; i < 3; i++) {
            int wave[SERVO_COUNT] = {90, 90, 90, 90, 160, 20};
            otto_move_servos(otto, 150, wave);
            wave[LEFT_HAND] = 45;
            wave[RIGHT_HAND] = 135;
            otto_move_servos(otto, 150, wave);
        }
    }

    otto_home(otto, true);
}

void otto_hand_wave(otto_t *otto, int dir) {
    if (!otto->has_hands) {
        return;
    }
    otto_hand_wave_internal(otto, dir);
}

void otto_windmill(otto_t *otto, float steps, int period, int amplitude) {
    if (!otto->has_hands) {
        return;
    }

    for (int i = 0; i < (int)(steps * 8); i++) {
        int pos = 90 + (i % 2 == 0 ? amplitude : -amplitude);
        int windmill[SERVO_COUNT] = {90, 90, 90, 90, pos, 180 - pos};
        otto_move_servos(otto, period / 8, windmill);
    }
}

void otto_takeoff(otto_t *otto, float steps, int period, int amplitude) {
    if (!otto->has_hands) {
        return;
    }

    otto_home(otto, true);

    for (int i = 0; i < (int)(steps * 4); i++) {
        int up[SERVO_COUNT] = {90, 90, 90, 90, 90 + amplitude, 90 - amplitude};
        int down[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
        otto_move_servos(otto, period / 4, up);
        otto_move_servos(otto, period / 4, down);
    }
}

void otto_fitness(otto_t *otto, float steps, int period, int amplitude) {
    if (!otto->has_hands) {
        return;
    }

    int target[SERVO_COUNT] = {90, 90, 90, 0, 160, 135};
    otto_move_servos(otto, 100, target);
    target[LEFT_FOOT] = 20;
    otto_move_servos(otto, 400, target);
    vTaskDelay(pdMS_TO_TICKS(2000));

    for (int i = 0; i < (int)steps; i++) {
        int fit1[SERVO_COUNT] = {90, 90, 20, 90, 160, 135};
        int fit2[SERVO_COUNT] = {90, 90, 20, 90, 160, 135 - amplitude};
        otto_move_servos(otto, period / 2, fit1);
        otto_move_servos(otto, period / 2, fit2);
    }
}

void otto_greeting(otto_t *otto, int dir, float steps) {
    if (!otto->has_hands) {
        return;
    }

    if (dir == LEFT) {
        int target[SERVO_COUNT] = {90, 90, 150, 150, 45, 135};
        otto_move_servos(otto, 400, target);
        for (int i = 0; i < (int)steps; i++) {
            int greet[SERVO_COUNT] = {90, 90, 150, 150, 160, 135};
            otto_move_servos(otto, 300, greet);
            greet[LEFT_HAND] = 100;
            otto_move_servos(otto, 300, greet);
        }
    } else {
        int target[SERVO_COUNT] = {90, 90, 30, 30, 45, 135};
        otto_move_servos(otto, 400, target);
        for (int i = 0; i < (int)steps; i++) {
            int greet[SERVO_COUNT] = {90, 90, 30, 30, 45, 20};
            otto_move_servos(otto, 300, greet);
            greet[RIGHT_HAND] = 70;
            otto_move_servos(otto, 300, greet);
        }
    }
}

void otto_shy(otto_t *otto, int dir, float steps) {
    if (!otto->has_hands) {
        return;
    }

    if (dir == LEFT) {
        int target[SERVO_COUNT] = {90, 90, 150, 150, 45, 135};
        otto_move_servos(otto, 400, target);
        for (int i = 0; i < (int)(steps * 4); i++) {
            int shy1[SERVO_COUNT] = {90, 90, 150, 150, 45, 135};
            int shy2[SERVO_COUNT] = {90, 90, 150, 150, 65, 115};
            otto_move_servos(otto, 150, shy1);
            otto_move_servos(otto, 150, shy2);
        }
    } else {
        int target[SERVO_COUNT] = {90, 90, 30, 30, 45, 135};
        otto_move_servos(otto, 400, target);
        for (int i = 0; i < (int)(steps * 4); i++) {
            int shy1[SERVO_COUNT] = {90, 90, 30, 30, 45, 135};
            int shy2[SERVO_COUNT] = {90, 90, 30, 30, 65, 115};
            otto_move_servos(otto, 150, shy1);
            otto_move_servos(otto, 150, shy2);
        }
    }
}

void otto_radio_calisthenics(otto_t *otto) {
    if (!otto->has_hands) {
        return;
    }

    const int period = 1000;

    for (int round = 0; round < 8; round++) {
        int c1[SERVO_COUNT] = {90, 90, 90, 90, 145, 45};
        int a1[SERVO_COUNT] = {0, 0, 0, 0, 45, 45};
        double ph1[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(-90)};
        otto_execute(otto, a1, (int[]){90, 90, 90, 90, 145, 45}, 1000, ph1, 1.0);

        int c2[SERVO_COUNT] = {90, 90, 115, 65, 90, 90};
        int a2[SERVO_COUNT] = {0, 0, 25, 25, 0, 0};
        double ph2[SERVO_COUNT] = {0, 0, DEG2RAD(90), DEG2RAD(-90), 0, 0};
        otto_execute(otto, a2, c2, 1000, ph2, 1.0);

        int c3[SERVO_COUNT] = {90, 90, 130, 130, 90, 90};
        int a3[SERVO_COUNT] = {0, 0, 0, 0, 20, 0};
        double ph3[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
        otto_execute(otto, a3, c3, 1000, ph3, 1.0);

        int c4[SERVO_COUNT] = {90, 90, 50, 50, 90, 90};
        int a4[SERVO_COUNT] = {0, 0, 0, 0, 0, 20};
        double ph4[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
        otto_execute(otto, a4, c4, 1000, ph4, 1.0);
    }
}

void otto_magic_circle(otto_t *otto) {
    if (!otto->has_hands) {
        return;
    }

    int A[SERVO_COUNT] = {30, 30, 30, 30, 50, 50};
    int O[SERVO_COUNT] = {0, 0, 5, -5, 0, 0};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(-90), DEG2RAD(-90), DEG2RAD(90)};

    otto_execute(otto, A, O, 700, phase_diff, 40);
}

void otto_showcase(otto_t *otto) {
    if (otto_get_rest_state(otto) == true) {
        otto_set_rest_state(otto, false);
    }

    otto_walk(otto, 3, 1000, FORWARD, 50);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (otto->has_hands) {
        otto_hand_wave_internal(otto, LEFT);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (otto->has_hands) {
        otto_radio_calisthenics(otto);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    otto_moonwalker(otto, 3, 900, 25, LEFT);
    vTaskDelay(pdMS_TO_TICKS(500));

    otto_swing(otto, 3, 1000, 30);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (otto->has_hands) {
        otto_takeoff(otto, 5, 300, 40);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (otto->has_hands) {
        otto_fitness(otto, 5, 1000, 25);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    otto_walk(otto, 3, 1000, BACKWARD, 50);
    vTaskDelay(pdMS_TO_TICKS(500));

    otto_home(otto, true);
}
