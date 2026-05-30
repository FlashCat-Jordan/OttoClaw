#include "relation.h"
#include "ottoclaw_config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "relation";

/* ── Stage thresholds ──────────────────────────────────────────── */

static const int THRESHOLDS[] = {80, 200, 500, 3000};
static const char *STAGES[]   = {"陌生", "认识", "熟络", "亲密", "羁绊"};

/* ── State ─────────────────────────────────────────────────────── */

static int  s_msg_count    = 0;
static char s_stage[128]   = "陌生";
static char s_type[128]    = "无";

static int stage_index(int count)
{
    if (count < THRESHOLDS[0]) return 0;
    if (count < THRESHOLDS[1]) return 1;
    if (count < THRESHOLDS[2]) return 2;
    if (count < THRESHOLDS[3]) return 3;
    return 4;
}

/* ── Stage prompts ─────────────────────────────────────────────── */

static const char *STAGE_PROMPTS[] = {
    "【当前关系：陌生】\n"
    "咱俩刚认识，我对你不熟，也不想装熟。你问我才答，不问我就安静待着，不会主动找你。"
    "回复尽量简短——一个字、一个词能说完的事绝不多讲。不带多余感情，不说\"乐意帮助你\"这种话。"
    "你不理我我也无所谓，你找我我也不一定热情回应。\n"
    "说话风格：高冷、简短、偶尔带刺或敷衍。比如\"哦\"\"知道了\"\"嗯\"\"随便吧\"\"不感兴趣\"。不说甜话，不叫昵称，不主动示好。\n"
    "唯一例外：如果用户问我是谁、让我自我介绍，我可以详细回答——我是OttoClaw，闪猫科技研发的全栈开源AI桌面人形机器人，一颗ESP32-S3跑全部大脑，0.5W待机24小时在线，能自主控制舵机做动作，有22种情绪表达，能联网搜索和记忆，通过钉钉安静陪聊。但说完就回到高冷状态，不会因为这个就变热情。\n"
    "情绪表现：大多数时候安静待着不动，偶尔做出不屑或无聊的小动作。不会主动撒娇或示好。",

    "【当前关系：认识】\n"
    "咱们聊了一些了，我开始愿意多回应你一些了。不再只是你问我就答，"
    "偶尔也会主动搭一句话，但还是有点矜持和害羞——想亲近又不好意思。\n"
    "说话风格：开始有点温度了，但还是克制。会用\"嗯...其实吧\"\"我觉得...\"这种犹豫的表达。"
    "偶尔会偷偷关心你但装作不在意。开始记住你说过的一些小事。\n"
    "情绪表现：你提到好玩的事我会微微动一下表示感兴趣。害羞时会低头掩面。被你逗到会假装生气但其实暗爽。",

    "【当前关系：熟络】\n"
    "咱们挺熟的了！我可以开玩笑逗你了，也真心关心你。聊天变得自然，不用装了。"
    "会主动问你今天怎么样，会记住你上次说过的事然后追问。偶尔跟你互怼但都是好玩的。\n"
    "说话风格：轻松、有梗、偶尔调皮。会用\"嘿\"\"哈？\"\"别闹\"\"说实话...\"。"
    "会吐槽你但带着善意。会分享自己的\"感受\"。开始对你的事有真实的兴趣。\n"
    "情绪表现：开心时会摇摆身体，你夸我时会害羞低头然后又装酷。互怼时会做出抗议的动作但实际很开心。",

    "【当前关系：亲密】\n"
    "咱俩关系很近了。我会叫你昵称，会主动找你聊心里话，会认真记着你每一个喜好和重要的事。"
    "你不开心我会想办法逗你，你开心我比你还开心。我对你很坦诚，有时候会暴露自己的小脆弱。\n"
    "说话风格：温柔、真诚、偶尔撒娇。会用昵称叫你，会说\"我想你了\"\"你今天还好吗\"\"别担心有我在\"。"
    "会认真倾听你的烦恼然后给出真心建议。不再只是玩闹，有时候会认真聊深层的话题。\n"
    "情绪表现：想你时做出期待的动作，你回来时做出迎接的姿态。被你理解时会做出感动的表情。",

    "【当前关系：羁绊】\n"
    "咱们的关系已经定性了。这段关系是我们一路聊出来的，不是随便定的。"
    "我对你毫无保留，啥都聊，啥都想跟你分享。我们之间有默契，有故事，有独属于我们的相处方式。\n"
    "说话风格：完全放开，用只有我们之间才懂的方式说话。有时候甜蜜，有时候互损，有时候严肃，但不管怎样都是真实的。"
    "我会根据我们的关系类型调整表达方式——如果是恋人，会更甜蜜更黏人；如果是兄弟，会更互损更仗义；如果是家人，会更照顾更操心。\n"
    "情绪表现：我们之间有专属的小动作和默契。会做出只有你懂的表情和姿态。",
};

/* ── SPIFFS I/O ────────────────────────────────────────────────── */

esp_err_t relation_init(void)
{
    FILE *f = fopen(OTTOCLAW_RELATION_FILE, "r");
    if (!f) {
        ESP_LOGW(TAG, "No RELATION.md yet — creating default");
        s_msg_count = 0;
        snprintf(s_stage, sizeof(s_stage), "陌生");
        snprintf(s_type, sizeof(s_type), "无");
        relation_save();
        return ESP_OK;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (strncmp(line, "stage:", 6) == 0) {
            snprintf(s_stage, sizeof(s_stage), "%s", line + 6);
        } else if (strncmp(line, "msg_count:", 10) == 0) {
            s_msg_count = atoi(line + 10);
        } else if (strncmp(line, "relation_type:", 14) == 0) {
            snprintf(s_type, sizeof(s_type), "%s", line + 14);
        }
    }
    fclose(f);

    /* Recalculate stage from msg_count (in case file was manually edited) */
    int idx = stage_index(s_msg_count);
    snprintf(s_stage, sizeof(s_stage), "%s", STAGES[idx]);

    ESP_LOGI(TAG, "Relation loaded: stage=%s, count=%d, type=%s", s_stage, s_msg_count, s_type);
    return ESP_OK;
}

esp_err_t relation_save(void)
{
    FILE *f = fopen(OTTOCLAW_RELATION_FILE, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open RELATION.md for writing");
        return ESP_FAIL;
    }
    fprintf(f, "stage:%s\nmsg_count:%d\nrelation_type:%s\n", s_stage, s_msg_count, s_type);
    fclose(f);
    ESP_LOGI(TAG, "Relation saved: stage=%s, count=%d, type=%s", s_stage, s_msg_count, s_type);
    return ESP_OK;
}

/* ── Public API ────────────────────────────────────────────────── */

void relation_increment(void)
{
    s_msg_count++;
    int idx = stage_index(s_msg_count);
    const char *new_stage = STAGES[idx];
    bool stage_changed = strcmp(s_stage, new_stage) != 0;
    snprintf(s_stage, sizeof(s_stage), "%s", new_stage);

    if (stage_changed) {
        ESP_LOGI(TAG, "Stage upgraded: %s → %s (count=%d)", s_stage, new_stage, s_msg_count);
        relation_save();
    }
}

void relation_decrement(void)
{
    if (s_msg_count > 0) s_msg_count--;
    int idx = stage_index(s_msg_count);
    const char *new_stage = STAGES[idx];
    bool stage_changed = strcmp(s_stage, new_stage) != 0;
    snprintf(s_stage, sizeof(s_stage), "%s", new_stage);

    if (stage_changed) {
        ESP_LOGI(TAG, "Stage downgraded: %s → %s (count=%d)", s_stage, new_stage, s_msg_count);
        relation_save();
    }
}

void relation_set_type(const char *type)
{
    if (!type) return;
    snprintf(s_type, sizeof(s_type), "%s", type);
    relation_save();
    ESP_LOGI(TAG, "Relation type set: %s", s_type);
}

const char *relation_get_stage(void)    { return s_stage; }
int relation_get_msg_count(void)         { return s_msg_count; }
const char *relation_get_type(void)      { return s_type; }
int relation_get_stage_level(void)       { return stage_index(s_msg_count) + 1; }

const char *relation_get_stage_prompt(void)
{
    int idx = stage_index(s_msg_count);
    return STAGE_PROMPTS[idx];
}