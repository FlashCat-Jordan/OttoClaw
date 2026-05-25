# OttoClaw Lite

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[中文](README.md) | [English](README_EN.md)**

> Every robot talks to you. OttoClaw never interrupts.
> It quietly messages you through DingTalk — greet it, command it, make it dance.
> AI truly controls the body: 6 servos, every joint precisely controllable, self-orchestrated.

Developed by **Shanmao Tech** · Open-source Lite edition

---

## What Makes It Different

All companion robots use voice — they speak, they play audio, they break your focus.

**OttoClaw takes a completely different path:**

- **Non-intrusive** — Messages through DingTalk / Feishu. Quiet like a cat. No sudden voice during your meeting. Check messages when free, send commands anytime.
- **AI is the soul, not a remote** — Not just "wave when I say wave". The LLM decides autonomously what motion, expression, and words. Say "dance" — AI choreographs: walk → wave → moonwalk → calisthenics → windmill → finish. Each routine is AI's own composition.
- **Precise joint control** — Beyond 22 predefined actions, AI can directly specify any servo angle. "Left hand to 45 degrees, right foot to 120 degrees" — it reaches that position exactly. 6 joints, 6 independent servos, AI can freely compose pose sequences.
- **6-servo full body** — Hands + legs + feet = 6 independent joints. Walk, turn, jump, sit, bend, wave, moonwalk, calisthenics, windmill, shy, fitness... each tunable: steps, speed, direction, amount.
- **0.5W always-on** — USB power, pure C / FreeRTOS, one ESP32-S3. 24/7, zero maintenance.

---

## Play With It

### Chat

Message on DingTalk — anything goes. Weather, news, code, stories, reminders. It searches the web, reads/writes files, remembers things.

```
You: "Weather in Hangzhou today?"
OttoClaw: [web_search] → "Hangzhou: sunny, 28°C — nice day for a walk"
```

### Make It Dance

One sentence, AI choreographs the whole routine:

```
You: "dance for me"
OttoClaw: → walk 3 → moonwalk → radio_calisthenics → swing → showcase → "Done dancing!"
```

### Precise Pose Control (Servo Sequences Lite)

New Lite capability — tell AI exactly where each joint should go:

```
You: "left hand to 160 degrees, right foot to 30 degrees"
OttoClaw: [self.otto.pose] → left hand 160° right foot 30° → "Pose achieved!"
```

```
You: "think for a moment"
OttoClaw: [self.otto.pose] → right hand 45° foot tilted → then reset → "Let me think..."
```

AI chains multiple pose calls into a sequence — head down → thinking → head up → answer. Each transition has controllable timing.

> **Lite note:** Open-source Lite edition provides Servo Sequences Lite — AI can specify any servo angle and compose pose sequences via multiple pose calls. Full Servo Sequences release (with self-programming capability for more AI-conscious physical expression) will come later.

### Live With It

OttoClaw remembers across restarts:

```
You: "Remember I love hotpot"
OttoClaw: [memory_write] → "Noted!"

You: "I passed my interview today!"
OttoClaw: [memory_append_today] → "Congrats! Logged."

(Days later)
OttoClaw: "Your interview was last week — how's the new job?"
```

---

## Actions

### Predefined (22 primitives)

walk · walk_backward · turn · jump · updown · swing · moonwalk · bend · shake_leg · sit · hands_up · hands_down · hand_wave · windmill · takeoff · fitness · greeting · shy · radio_calisthenics · magic_circle · showcase · home

Each tunable with steps, speed, direction, amount.

### Precise Pose (Servo Sequences Lite)

`self.otto.pose` — AI specifies 6 servo angles + transition time:

| Param | Description | Range | Default |
|-------|-------------|-------|---------|
| `left_leg` | Left leg servo angle | 0-180 | 90 |
| `right_leg` | Right leg servo angle | 0-180 | 90 |
| `left_foot` | Left foot servo angle | 0-180 | 90 |
| `right_foot` | Right foot servo angle | 0-180 | 90 |
| `left_hand` | Left hand servo angle | 0-180 | 45 |
| `right_hand` | Right hand servo angle | 0-180 | 135 |
| `time` | Transition time (ms) | 100-3000 | 700 |

90° = neutral. 45°/135° = natural hand rest. Lower angles = one side, higher = the other.

---

## Quick Start

### Prerequisites

- ESP32-S3 board (16MB Flash + 8MB PSRAM)
- Otto 6-servo robot kit
- USB Type-C **data** cable
- LLM API key

### Install ESP-IDF

```bash
git clone -b v5.5.2 --depth 1 https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && ./install.sh esp32s3
source ~/esp/esp-idf/export.sh   # Run this before every build
```

### Build & Flash

```bash
git clone https://github.com/1335328414/FlashCatClaw.git
cd FlashCatClaw/mimiclaw-main
cp main/ottoclaw_secrets.h.example main/ottoclaw_secrets.h
idf.py set-target esp32s3 && idf.py build && idf.py -p PORT flash
```

### First Config

No WiFi → auto portal mode. Connect phone to `OttoClaw-XXXX` hotspot → open http://192.168.4.1 → fill in settings → Save & Reboot.

Re-enter portal anytime: **hold BOOT button 2s** during startup.

---

## Configuration Guide

Two layers: compile-time defaults (`ottoclaw_secrets.h`) + runtime overrides (NVS/portal/CLI). Runtime wins.

### WiFi

**Portal:** Enter SSID + password → Save & Reboot.

**CLI:** `oc> wifi_set SSID PASSWORD && oc> restart`

Auto-fallback to portal mode if WiFi fails — no worries if you misconfigure.

### LLM (the key config)

**Anthropic Claude (default, needs proxy in China):**

1. [Anthropic Console](https://console.anthropic.com/) → Create API Key
2. `oc> set_model_provider anthropic && oc> set_model claude-sonnet-4-5 && oc> set_api_key sk-ant-api03-xxxxx`

**Qwen/DashScope (recommended for China, no proxy needed):**

1. [DashScope Console](https://dashscope.console.aliyun.com/) → Enable service → Create API Key
2. `oc> set_model_provider qwen && oc> set_model qwen-max && oc> set_api_key sk-xxxxx`

**DeepSeek (China, no proxy, good value):**

1. [DeepSeek Platform](https://platform.deepseek.com/) → Create API Key
2. `oc> set_model_provider deepseek && oc> set_model deepseek-chat && oc> set_api_key sk-xxxxx`

| Provider | Value | Note |
|----------|-------|------|
| OpenAI | `openai` | Classic, needs proxy |
| Gemini | `gemini` | Google, needs proxy |
| Groq | `groq` | Fastest inference |
| Zhipu (GLM) | `zhipu` | Strong Chinese |
| vLLM | `vllm` | Self-hosted |

### DingTalk

1. [DingTalk Developer Platform](https://open-dev.dingtalk.com/) → Create app → Robot type
2. Copy **App Key** and **App Secret**
3. **Important:** Set message receiving mode to **Stream** (not HTTP callback)
4. `oc> set_dingtalk dingxxxx secretxxx`

Stream mode = direct WebSocket connection to DingTalk. No public server needed.

### Web Search

1. [Bailian Platform](https://bailian.console.aliyun.com/) → Create app → Search-enhanced type
2. Copy **App ID** and **API Key**
3. `oc> set_search_key sk-xxxxx && oc> set_bailian_app_id 758d9af4...`

### Proxy (for Claude/OpenAI in China)

`oc> set_proxy 192.168.1.83 7897` (Clash/V2Ray HTTP port)

`oc> clear_proxy` to remove

---

## CLI

USB serial, 115200 baud, `oc>` prompt:

```
oc> wifi_set <ssid> <pass>      oc> set_dingtalk <key> <secret>
oc> set_api_key <key>           oc> set_model <model>
oc> set_model_provider <p>      oc> set_bailian_app_id <id>
oc> set_search_key <key>        oc> set_proxy <host> <port>
oc> clear_proxy                 oc> config_show
oc> config_reset                oc> restart
oc> wifi_status                 oc> wifi_scan
oc> memory_read                 oc> memory_write "content"
oc> heap_info                   oc> session_list
```

---

## Memory

All data as plain text on SPIFFS. AI reads/writes:

- `SOUL.md` — Personality
- `USER.md` — User preferences
- `MEMORY.md` — Long-term memory
- `YYYY-MM-DD.md` — Daily auto-notes
- `<chat_id>.jsonl` — Chat history

---

## Architecture

Pure C / FreeRTOS · Dual-core (Core 0 I/O, Core 1 agent) · Anthropic tool use ReAct loop · SPIFFS local storage · 6 servo LEDC PWM

**[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** · **[docs/TODO.md](docs/TODO.md)**

---

## License

MIT

## Acknowledgments

Inspired by [mimiclaw](https://github.com/memovai/mimiclaw), [OpenClaw](https://github.com/openclaw/openclaw), and [Nanobot](https://github.com/HKUDS/nanobot).