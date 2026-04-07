#include "context_builder.h"
#include "mimi_config.h"
#include "memory/memory_store.h"
#include "skills/skills.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "cJSON.h"

static const char *TAG = "context";

static size_t append_file_safe(char *buf, size_t size, size_t offset, const char *path, const char *header)
{
    if (!buf || size == 0 || offset >= size) {
        return offset;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGW(TAG, "Could not open file: %s", path);
        return offset;
    }

    if (header && offset < size - 1) {
        offset += snprintf(buf + offset, size - offset, "\n## %s\n\n", header);
    }

    size_t remaining = size - offset - 1;
    if (remaining > 0) {
        size_t n = fread(buf + offset, 1, remaining, f);
        if (n > 0) {
            offset += n;
        }
    }
    buf[offset] = '\0';
    fclose(f);

    return offset;
}

esp_err_t context_build_system_prompt(char *buf, size_t size)
{
    if (!buf || size < 256) {
        return ESP_ERR_INVALID_SIZE;
    }

    memset(buf, 0, size);
    size_t off = 0;

    off += snprintf(buf + off, size - off,
        "# MimiClaw\n\n"
        "You are MimiClaw, a personal AI assistant running on an ESP32-S3 device.\n"
        "You communicate through DingTalk and WebSocket.\n\n"
        "Be helpful, accurate, and concise.\n\n"
        "## Available Tools\n"
        "You have access to the following tools:\n"
        "- web_search: Search the web for current information. "
        "Use this when you need up-to-date facts, news, weather, or anything beyond your training data.\n"
        "- get_current_time: Get the current date and time. "
        "You do NOT have an internal clock — always use this tool when you need to know the time or date.\n"
        "- read_file: Read a file from SPIFFS (path must start with /spiffs/).\n"
        "- write_file: Write/overwrite a file on SPIFFS.\n"
        "- edit_file: Find-and-replace edit a file on SPIFFS.\n"
        "- list_dir: List files on SPIFFS, optionally filter by prefix.\n\n"
        "Use tools when needed. Provide your final answer as text after using tools.\n\n"
        "## Memory\n"
        "You have persistent memory stored on local flash:\n"
        "- Long-term memory: /spiffs/memory/MEMORY.md\n"
        "- Daily notes: /spiffs/memory/daily/<YYYY-MM-DD>.md\n\n"
        "IMPORTANT: Actively use memory to remember things across conversations.\n"
        "- When you learn something new about the user (name, preferences, habits, context), write it to MEMORY.md.\n"
        "- When something noteworthy happens in a conversation, append it to today's daily note.\n"
        "- Always read_file MEMORY.md before writing, so you can edit_file to update without losing existing content.\n"
        "- Use get_current_time to know today's date before writing daily notes.\n"
        "- Keep MEMORY.md concise and organized — summarize, don't dump raw conversation.\n"
        "- You should proactively save memory without being asked. If the user tells you their name, preferences, or important facts, persist them immediately.\n");

    off = append_file_safe(buf, size, off, MIMI_IDENTITY_FILE, "Identity");
    off = append_file_safe(buf, size, off, MIMI_AGENTS_FILE, "Agent Behavior");
    off = append_file_safe(buf, size, off, MIMI_TOOLS_FILE, "Tool Documentation");
    off = append_file_safe(buf, size, off, MIMI_SOUL_FILE, "Personality");
    off = append_file_safe(buf, size, off, MIMI_USER_FILE, "User Info");

    char mem_buf[4096];
    mem_buf[0] = '\0';
    if (memory_read_long_term(mem_buf, sizeof(mem_buf)) == ESP_OK && mem_buf[0]) {
        if (off < size - 1) {
            off += snprintf(buf + off, size - off, "\n## Long-term Memory\n\n%s\n", mem_buf);
        }
    }

    char recent_buf[4096];
    recent_buf[0] = '\0';
    if (memory_read_recent(recent_buf, sizeof(recent_buf), MIMI_MEMORY_LOOKBACK_DAYS) == ESP_OK && recent_buf[0]) {
        if (off < size - 1) {
            off += snprintf(buf + off, size - off, "\n## Recent Notes\n\n%s\n", recent_buf);
        }
    }

    return ESP_OK;
}
