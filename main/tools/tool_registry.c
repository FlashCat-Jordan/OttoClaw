#include "tool_registry.h"
#include "tools/tool_web_search.h"
#include "tools/tool_get_time.h"
#include "tools/tool_files.h"
#include "tools/tool_otto.h"
#include "../memory/memory_store.h"

#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "tools";

#define MAX_TOOLS 16

static esp_err_t tool_memory_read_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;
    esp_err_t err = memory_read_long_term(output, output_size);
    if (err == ESP_ERR_NOT_FOUND || output[0] == '\0') {
        snprintf(output, output_size, "(MEMORY.md is empty)");
    }
    return ESP_OK;
}

static esp_err_t tool_memory_write_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *input = cJSON_Parse(input_json);
    if (!input) {
        snprintf(output, output_size, "Error: Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *content = cJSON_GetObjectItem(input, "content");
    if (!cJSON_IsString(content)) {
        cJSON_Delete(input);
        snprintf(output, output_size, "Error: Missing 'content' field");
        return ESP_FAIL;
    }

    esp_err_t err = memory_write_long_term(content->valuestring);
    cJSON_Delete(input);

    if (err == ESP_OK) {
        snprintf(output, output_size, "Successfully wrote to MEMORY.md");
        return ESP_OK;
    } else {
        snprintf(output, output_size, "Error: Failed to write to MEMORY.md");
        return err;
    }
}

static esp_err_t tool_memory_append_today_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *input = cJSON_Parse(input_json);
    if (!input) {
        snprintf(output, output_size, "Error: Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *note = cJSON_GetObjectItem(input, "note");
    if (!cJSON_IsString(note)) {
        cJSON_Delete(input);
        snprintf(output, output_size, "Error: Missing 'note' field");
        return ESP_FAIL;
    }

    esp_err_t err = memory_append_today(note->valuestring);
    cJSON_Delete(input);

    if (err == ESP_OK) {
        snprintf(output, output_size, "Successfully appended to today's memory");
        return ESP_OK;
    } else {
        snprintf(output, output_size, "Error: Failed to append to daily memory");
        return err;
    }
}

static mimi_tool_t s_tools[MAX_TOOLS];
static int s_tool_count = 0;
static char *s_tools_json = NULL;  /* cached JSON array string */

void tool_registry_register(const mimi_tool_t *tool)
{
    if (s_tool_count >= MAX_TOOLS) {
        ESP_LOGE(TAG, "Tool registry full");
        return;
    }
    s_tools[s_tool_count++] = *tool;
    ESP_LOGI(TAG, "Registered tool: %s", tool->name);
}

static void build_tools_json(void)
{
    cJSON *arr = cJSON_CreateArray();

    for (int i = 0; i < s_tool_count; i++) {
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", s_tools[i].name);
        cJSON_AddStringToObject(tool, "description", s_tools[i].description);

        cJSON *schema = cJSON_Parse(s_tools[i].input_schema_json);
        if (schema) {
            cJSON_AddItemToObject(tool, "input_schema", schema);
        }

        cJSON_AddItemToArray(arr, tool);
    }

    free(s_tools_json);
    s_tools_json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    ESP_LOGI(TAG, "Tools JSON built (%d tools)", s_tool_count);
}

esp_err_t tool_registry_init(void)
{
    s_tool_count = 0;

    /* Register web_search */
    tool_web_search_init();

    mimi_tool_t ws = {
        .name = "web_search",
        .description = "Search the web for current information. Use this when you need up-to-date facts, news, weather, or anything beyond your training data.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"query\":{\"type\":\"string\",\"description\":\"The search query\"}},"
            "\"required\":[\"query\"]}",
        .execute = tool_web_search_execute,
    };
    tool_registry_register(&ws);

    /* Register get_current_time */
    mimi_tool_t gt = {
        .name = "get_current_time",
        .description = "Get the current date and time. Also sets the system clock. Call this when you need to know what time or date it is.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{},"
            "\"required\":[]}",
        .execute = tool_get_time_execute,
    };
    tool_registry_register(&gt);

    /* Register read_file */
    mimi_tool_t rf = {
        .name = "read_file",
        .description = "Read a file from SPIFFS storage. Path must start with /spiffs/.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Absolute path starting with /spiffs/\"}},"
            "\"required\":[\"path\"]}",
        .execute = tool_read_file_execute,
    };
    tool_registry_register(&rf);

    /* Register write_file */
    mimi_tool_t wf = {
        .name = "write_file",
        .description = "Write or overwrite a file on SPIFFS storage. Path must start with /spiffs/.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Absolute path starting with /spiffs/\"},"
            "\"content\":{\"type\":\"string\",\"description\":\"File content to write\"}},"
            "\"required\":[\"path\",\"content\"]}",
        .execute = tool_write_file_execute,
    };
    tool_registry_register(&wf);

    /* Register edit_file */
    mimi_tool_t ef = {
        .name = "edit_file",
        .description = "Find and replace text in a file on SPIFFS. Replaces first occurrence of old_string with new_string.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Absolute path starting with /spiffs/\"},"
            "\"old_string\":{\"type\":\"string\",\"description\":\"Text to find\"},"
            "\"new_string\":{\"type\":\"string\",\"description\":\"Replacement text\"}},"
            "\"required\":[\"path\",\"old_string\",\"new_string\"]}",
        .execute = tool_edit_file_execute,
    };
    tool_registry_register(&ef);

    /* Register list_dir */
    mimi_tool_t ld = {
        .name = "list_dir",
        .description = "List files on SPIFFS storage, optionally filtered by path prefix.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"prefix\":{\"type\":\"string\",\"description\":\"Optional path prefix filter, e.g. /spiffs/memory/\"}},"
            "\"required\":[]}",
        .execute = tool_list_dir_execute,
    };
    tool_registry_register(&ld);

    /* Register memory_read */
    mimi_tool_t mr = {
        .name = "memory_read",
        .description = "Read the current contents of long-term memory (MEMORY.md). ALWAYS call this before memory_write to avoid overwriting existing memories.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{},"
            "\"required\":[]}",
        .execute = tool_memory_read_execute,
    };
    tool_registry_register(&mr);

    /* Register memory_write */
    mimi_tool_t mw = {
        .name = "memory_write",
        .description = "Write content to long-term memory (MEMORY.md). Use this to persist important information that should be remembered across conversations.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"content\":{\"type\":\"string\",\"description\":\"Content to write to MEMORY.md\"}},"
            "\"required\":[\"content\"]}",
        .execute = tool_memory_write_execute,
    };
    tool_registry_register(&mw);

    /* Register memory_append_today */
    mimi_tool_t mat = {
        .name = "memory_append_today",
        .description = "Append a note to today's daily memory file (YYYY-MM-DD.md). Use this to log events, notes, or daily summaries.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"note\":{\"type\":\"string\",\"description\":\"Note to append to today's memory file\"}},"
            "\"required\":[\"note\"]}",
        .execute = tool_memory_append_today_execute,
    };
    tool_registry_register(&mat);

    /* Register otto robot tools */
    tool_otto_register();

    build_tools_json();

    ESP_LOGI(TAG, "Tool registry initialized");
    return ESP_OK;
}

const char *tool_registry_get_tools_json(void)
{
    return s_tools_json;
}

esp_err_t tool_registry_execute(const char *name, const char *input_json,
                                char *output, size_t output_size)
{
    for (int i = 0; i < s_tool_count; i++) {
        if (strcmp(s_tools[i].name, name) == 0) {
            ESP_LOGI(TAG, "Executing tool: %s", name);
            return s_tools[i].execute(input_json, output, output_size);
        }
    }

    ESP_LOGW(TAG, "Unknown tool: %s", name);
    snprintf(output, output_size, "Error: unknown tool '%s'", name);
    return ESP_ERR_NOT_FOUND;
}
