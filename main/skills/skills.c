#include "skills.h"
#include "ottoclaw_config.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"

static const char *TAG = "skills";

#define MAX_SKILLS 10
#define MAX_SKILL_NAME 64
#define MAX_SKILL_SIZE 2048

typedef struct {
    char name[MAX_SKILL_NAME];
    char content[MAX_SKILL_SIZE];
} skill_t;

static skill_t skills[MAX_SKILLS];
static int skill_count = 0;

static esp_err_t load_skill_file(const char *path)
{
    if (skill_count >= MAX_SKILLS) {
        ESP_LOGW(TAG, "Maximum skills reached, skipping: %s", path);
        return ESP_ERR_NO_MEM;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGW(TAG, "Failed to open skill file: %s", path);
        return ESP_FAIL;
    }

    struct stat st;
    if (stat(path, &st) == 0 && st.st_size >= MAX_SKILL_SIZE) {
        ESP_LOGW(TAG, "Skill file too large, skipping: %s", path);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t n = fread(skills[skill_count].content, 1, MAX_SKILL_SIZE - 1, f);
    skills[skill_count].content[n] = '\0';
    fclose(f);

    const char *filename = strrchr(path, '/');
    if (filename) {
        filename++;
        size_t len = strlen(filename);
        if (len >= MAX_SKILL_NAME) {
            len = MAX_SKILL_NAME - 1;
        }
        memcpy(skills[skill_count].name, filename, len);
        skills[skill_count].name[len] = '\0';
    } else {
        strncpy(skills[skill_count].name, path, MAX_SKILL_NAME - 1);
        skills[skill_count].name[MAX_SKILL_NAME - 1] = '\0';
    }

    ESP_LOGI(TAG, "Loaded skill: %s (%d bytes)", skills[skill_count].name, (int)n);
    skill_count++;

    return ESP_OK;
}

esp_err_t skills_init(void)
{
    skill_count = 0;

    DIR *dir = opendir(OTTOCLAW_SPIFFS_SKILLS_DIR);
    if (!dir) {
        ESP_LOGW(TAG, "Skills directory not found: %s", OTTOCLAW_SPIFFS_SKILLS_DIR);
        return ESP_ERR_NOT_FOUND;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && strcasecmp(ext, ".md") == 0) {
                char path[512];
                int ret = snprintf(path, sizeof(path), "%s/%s", OTTOCLAW_SPIFFS_SKILLS_DIR, entry->d_name);
                if (ret > 0 && (size_t)ret < sizeof(path)) {
                    load_skill_file(path);
                } else {
                    ESP_LOGW(TAG, "Path too long for skill: %s", entry->d_name);
                }
            }
        }
    }

    closedir(dir);
    ESP_LOGI(TAG, "Skills initialized: %d skills loaded", skill_count);

    return ESP_OK;
}

esp_err_t skills_get_content(char *buf, size_t size)
{
    if (!buf || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t off = 0;
    off += snprintf(buf + off, size - off, "\n## Skills\n\n");

    for (int i = 0; i < skill_count; i++) {
        off += snprintf(buf + off, size - off, "### %s\n\n%s\n\n",
                        skills[i].name, skills[i].content);
    }

    return ESP_OK;
}

int skills_get_count(void)
{
    return skill_count;
}
