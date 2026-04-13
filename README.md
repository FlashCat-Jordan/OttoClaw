# MiaomiaoClaw: Pocket AI Assistant on a $5 Chip

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![DeepWiki](https://img.shields.io/badge/DeepWiki-mimiclaw-blue.svg)](https://deepwiki.com/memovai/mimiclaw)
[![Discord](https://img.shields.io/badge/Discord-mimiclaw-5865F2?logo=discord&logoColor=white)](https://discord.gg/r8ZxSvB8Yr)
[![X](https://img.shields.io/badge/X-@ssslvky-black?logo=x)](https://x.com/ssslvky)

**[English](README.md) | [中文](README_CN.md)**

<p align="center">
  <img src="assets/banner.png" alt="MiaomiaoClaw" width="480" />
</p>

**An ESP32-S3 AI assistant with config portal, DingTalk chat, WebSocket access, LCD expressions, and Otto robot motions — all in pure C.**

MiaomiaoClaw turns a tiny ESP32-S3 board into a personal AI assistant. Power it by USB, connect it to WiFi, finish setup in the built-in config portal, and then talk to it through DingTalk or WebSocket. Claude runs through an on-device agent loop, uses tools, reads local memory, and drives both the LCD and Otto feedback on the same board.

## Meet MiaomiaoClaw

- **Tiny** — No Linux, no Node.js, no bloat — just pure C
- **Connected** — Built-in config portal, DingTalk bot, and WebSocket gateway
- **Embodied** — LCD moods and Otto movement feedback
- **Loyal** — Learns from memory, remembers across reboots
- **Energetic** — USB power, 0.5 W, runs 24/7

## How It Works

![](assets/mimiclaw.png)

A channel module receives a message from DingTalk or WebSocket, normalizes it into the internal message bus, and hands it to the agent loop. The ESP32-S3 builds context from config + memory + sessions, calls the LLM with tool use, saves the result locally, and sends the reply back through the same channel. During chat it can also update LCD mood and trigger short Otto actions.

## Quick Start

### What You Need

- An **ESP32-S3 dev board** with 16 MB flash and 8 MB PSRAM
- A **USB Type-C cable**
- An **LLM API key** — Anthropic by default, or another configured provider
- Optional: **DingTalk app credentials**
- Optional: **Brave Search API key**

### Install

```bash
# You need ESP-IDF v5.5+ installed first:
# https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/

git clone https://github.com/memovai/mimiclaw.git
cd mimiclaw

idf.py set-target esp32s3
```

### Configure

MiaomiaoClaw uses a **two-layer config** system: build-time defaults in `mimi_secrets.h`, with runtime overrides stored in NVS. You can configure the device from the serial CLI or from the built-in web portal.

```bash
cp main/mimi_secrets.h.example main/mimi_secrets.h
```

Edit `main/mimi_secrets.h` if you want build-time defaults:

```c
#define MIMI_SECRET_WIFI_SSID              "YourWiFiName"
#define MIMI_SECRET_WIFI_PASS              "YourWiFiPassword"
#define MIMI_SECRET_DINGTALK_APP_KEY       "dingxxxx"
#define MIMI_SECRET_DINGTALK_APP_SECRET    "xxxx"
#define MIMI_SECRET_API_KEY                "sk-ant-api03-xxxxx"
#define MIMI_SECRET_MODEL_PROVIDER         "anthropic"
#define MIMI_SECRET_MODEL                  "claude-opus-4-5"
#define MIMI_SECRET_API_BASE_URL           ""
#define MIMI_SECRET_SEARCH_KEY             ""
#define MIMI_SECRET_PROXY_HOST             ""
#define MIMI_SECRET_PROXY_PORT             ""
```

Then build and flash:

```bash
idf.py fullclean && idf.py build
idf.py -p PORT flash monitor
```

### First Boot

If no WiFi credentials are stored, or if you hold the BOOT button during startup, the device enters **config portal** mode:

1. Connect your phone or computer to the hotspot `MiaomiaoClaw-XXXX`
2. Open `http://192.168.4.1`
3. Configure WiFi, LLM, DingTalk, proxy/search, and test Otto motions
4. Save and reboot

### CLI Commands

Connect via serial to configure or debug.

**Runtime config** (saved to NVS, overrides build-time defaults):

```
mimi> wifi_set MySSID MyPassword
mimi> set_dingtalk dingxxx secretxxx
mimi> set_api_key sk-ant-api03-...
mimi> set_model claude-sonnet-4-5
mimi> set_model_provider anthropic
mimi> set_proxy 127.0.0.1 7897
mimi> clear_proxy
mimi> set_search_key BSA...
mimi> config_show
mimi> config_reset
```

**Debug & maintenance:**

```
mimi> wifi_status
mimi> wifi_scan
mimi> memory_read
mimi> memory_write "content"
mimi> heap_info
mimi> session_list
mimi> session_clear 12345
mimi> restart
```

## Channels

- **DingTalk** — primary mobile chat channel in this fork
- **WebSocket gateway** — port `18789`, useful for LAN UI or bridge services
- **Serial CLI** — local maintenance and debugging

## Memory

MiaomiaoClaw stores everything as plain text files you can read and edit:

| File | What it is |
|------|------------|
| `SOUL.md` | The bot's personality |
| `USER.md` | User profile and preferences |
| `IDENTITY.md` | Product/identity bootstrap text |
| `AGENTS.md` | Agent behavior guidance |
| `TOOLS.md` | Tool guidance |
| `MEMORY.md` | Long-term memory |
| `YYYY-MM-DD.md` | Daily notes |
| `<chat_id>.jsonl` | Session history |

## Tools

MiaomiaoClaw uses Anthropic-style tool use / ReAct loops.

| Tool | Description |
|------|-------------|
| `web_search` | Search the web via Brave Search API |
| `get_current_time` | Fetch current date/time and set the system clock |
| `read_file` / `write_file` / `edit_file` / `list_dir` | SPIFFS file access |
| `memory_write` / `memory_append_today` | Persistent memory operations |
| `self.otto.action` | Trigger robot motions |

## Also Included

- **Config portal** — AP mode + web UI for first-time setup
- **LCD mood system** — thinking/speaking/base moods during chat
- **Otto robot actions** — short expression motions and motion test page
- **Dual-core layout** — I/O and agent processing split across CPU cores
- **HTTP proxy** — CONNECT tunnel support for restricted networks
- **Cron / heartbeat services** — background services available in the firmware
- **Voice transcription module** — present in firmware, not yet fully wired into the main chat flow

## For Developers

Technical details live in the `docs/` folder:

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — current system design and module map
- **[docs/TODO.md](docs/TODO.md)** — current optimization roadmap for this fork

## License

MIT

## Acknowledgments

Inspired by [OpenClaw](https://github.com/openclaw/openclaw) and [Nanobot](https://github.com/HKUDS/nanobot). MiaomiaoClaw adapts the core AI agent architecture for embedded hardware while extending it toward a more embodied device experience.

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=memovai/mimiclaw&type=Date)](https://star-history.com/#memovai/mimiclaw&Date)
