#pragma once

/* OttoClaw Global Configuration */

/* Build-time secrets (highest priority, override NVS) */
#if __has_include("ottoclaw_secrets.h")
#include "ottoclaw_secrets.h"
#endif

#ifndef OTTOCLAW_SECRET_WIFI_SSID
#define OTTOCLAW_SECRET_WIFI_SSID       ""
#endif
#ifndef OTTOCLAW_SECRET_WIFI_PASS
#define OTTOCLAW_SECRET_WIFI_PASS       ""
#endif
#ifndef OTTOCLAW_SECRET_DINGTALK_APP_KEY
#define OTTOCLAW_SECRET_DINGTALK_APP_KEY    ""
#endif
#ifndef OTTOCLAW_SECRET_DINGTALK_APP_SECRET
#define OTTOCLAW_SECRET_DINGTALK_APP_SECRET ""
#endif
#ifndef OTTOCLAW_SECRET_API_KEY
#define OTTOCLAW_SECRET_API_KEY         ""
#endif
#ifndef OTTOCLAW_SECRET_MODEL
#define OTTOCLAW_SECRET_MODEL           ""
#endif
#ifndef OTTOCLAW_SECRET_MODEL_PROVIDER
#define OTTOCLAW_SECRET_MODEL_PROVIDER  "anthropic"
#endif
#ifndef OTTOCLAW_SECRET_API_BASE_URL
#define OTTOCLAW_SECRET_API_BASE_URL    ""
#endif
#ifndef OTTOCLAW_SECRET_PROXY_HOST
#define OTTOCLAW_SECRET_PROXY_HOST      ""
#endif
#ifndef OTTOCLAW_SECRET_PROXY_PORT
#define OTTOCLAW_SECRET_PROXY_PORT      ""
#endif
#ifndef OTTOCLAW_SECRET_SEARCH_KEY
#define OTTOCLAW_SECRET_SEARCH_KEY      ""
#endif
#ifndef OTTOCLAW_SECRET_ALLOW_FROM
#define OTTOCLAW_SECRET_ALLOW_FROM      ""
#endif
#ifndef OTTOCLAW_SECRET_BAILIAN_APP_ID
#define OTTOCLAW_SECRET_BAILIAN_APP_ID  ""
#endif

/* WiFi */
#define OTTOCLAW_WIFI_MAX_RETRY          10
#define OTTOCLAW_WIFI_RETRY_BASE_MS      1000
#define OTTOCLAW_WIFI_RETRY_MAX_MS       30000

/* DingTalk Bot */
#define OTTOCLAW_DINGTALK_POLL_INTERVAL_MS 5000
#define OTTOCLAW_DINGTALK_POLL_STACK       (12 * 1024)
#define OTTOCLAW_DINGTALK_POLL_PRIO        5
#define OTTOCLAW_DINGTALK_POLL_CORE        0

/* Agent Loop */
#define OTTOCLAW_AGENT_STACK             (12 * 1024)
#define OTTOCLAW_AGENT_PRIO              6
#define OTTOCLAW_AGENT_CORE              1
#define OTTOCLAW_AGENT_MAX_HISTORY       20
#define OTTOCLAW_AGENT_MAX_TOOL_ITER     10
#define OTTOCLAW_MAX_TOOL_CALLS          4

/* Timezone (POSIX TZ format) */
#define OTTOCLAW_TIMEZONE                "CST-8"

/* LLM */
#define OTTOCLAW_LLM_DEFAULT_MODEL       "claude-opus-4-5"
#define OTTOCLAW_LLM_PROVIDER_DEFAULT    "anthropic"
#define OTTOCLAW_LLM_MAX_TOKENS          4096
#define OTTOCLAW_LLM_API_URL             "https://api.anthropic.com/v1/messages"
#define OTTOCLAW_OPENAI_COMPAT_API_URL   "https://api.openai.com/v1/chat/completions"
#define OTTOCLAW_LLM_API_VERSION         "2023-06-01"
#define OTTOCLAW_LLM_STREAM_BUF_SIZE     (32 * 1024)

/* Message Bus */
#define OTTOCLAW_BUS_QUEUE_LEN           8
#define OTTOCLAW_OUTBOUND_STACK          (8 * 1024)
#define OTTOCLAW_OUTBOUND_PRIO           5
#define OTTOCLAW_OUTBOUND_CORE           0

/* Memory / SPIFFS */
#define OTTOCLAW_SPIFFS_BASE             "/spiffs"
#define OTTOCLAW_SPIFFS_CONFIG_DIR       "/spiffs/config"
#define OTTOCLAW_SPIFFS_MEMORY_DIR       "/spiffs/memory"
#define OTTOCLAW_SPIFFS_SESSION_DIR      "/spiffs"
#define OTTOCLAW_SPIFFS_AGENTS_DIR       "/spiffs/agents"
#define OTTOCLAW_SPIFFS_SKILLS_DIR       "/spiffs/skills"
#define OTTOCLAW_MEMORY_FILE             "/spiffs/memory/MEMORY.md"
#define OTTOCLAW_SOUL_FILE               "/spiffs/config/SOUL.md"
#define OTTOCLAW_RELATION_FILE           "/spiffs/config/RELATION.md"
#define OTTOCLAW_USER_FILE               "/spiffs/config/USER.md"
#define OTTOCLAW_AGENTS_FILE             "/spiffs/config/AGENTS.md"
#define OTTOCLAW_TOOLS_FILE              "/spiffs/config/TOOLS.md"
#define OTTOCLAW_IDENTITY_FILE           "/spiffs/config/IDENTITY.md"
#define OTTOCLAW_CONTEXT_BUF_SIZE        (16 * 1024)
#define OTTOCLAW_SESSION_MAX_MSGS        20
#define OTTOCLAW_MEMORY_LOOKBACK_DAYS    7

/* WebSocket Gateway */
#define OTTOCLAW_WS_PORT                 18789
#define OTTOCLAW_WS_MAX_CLIENTS          4

/* Serial CLI */
#define OTTOCLAW_CLI_STACK               (4 * 1024)
#define OTTOCLAW_CLI_PRIO                3
#define OTTOCLAW_CLI_CORE                0

/* NVS Namespaces */
#define OTTOCLAW_NVS_WIFI                "wifi_config"
#define OTTOCLAW_NVS_DINGTALK            "dingtalk_cfg"
#define OTTOCLAW_NVS_LLM                 "llm_config"
#define OTTOCLAW_NVS_PROXY               "proxy_config"
#define OTTOCLAW_NVS_SEARCH              "search_config"

/* NVS Keys */
#define OTTOCLAW_NVS_KEY_SSID            "ssid"
#define OTTOCLAW_NVS_KEY_PASS            "password"
#define OTTOCLAW_NVS_KEY_DINGTALK_KEY    "app_key"
#define OTTOCLAW_NVS_KEY_DINGTALK_SECRET "app_secret"
#define OTTOCLAW_NVS_KEY_API_KEY         "api_key"
#define OTTOCLAW_NVS_KEY_MODEL           "model"
#define OTTOCLAW_NVS_KEY_PROVIDER        "provider"
#define OTTOCLAW_NVS_KEY_BASE_URL        "base_url"
#define OTTOCLAW_NVS_KEY_PROXY_HOST      "host"
#define OTTOCLAW_NVS_KEY_PROXY_PORT      "port"
#define OTTOCLAW_NVS_KEY_SEARCH_KEY      "search_key"
#define OTTOCLAW_NVS_KEY_WHISPER_BASE_URL "whisper_url"
#define OTTOCLAW_NVS_KEY_BAILIAN_APP_ID   "bailian_app_id"
