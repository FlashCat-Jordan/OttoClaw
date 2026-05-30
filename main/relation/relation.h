#pragma once

#include "esp_err.h"

esp_err_t relation_init(void);
void relation_increment(void);
void relation_decrement(void);
void relation_set_type(const char *type);
const char *relation_get_stage(void);
int relation_get_msg_count(void);
const char *relation_get_type(void);
const char *relation_get_stage_prompt(void);
int relation_get_stage_level(void);
esp_err_t relation_save(void);