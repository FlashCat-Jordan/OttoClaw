# OttoClaw Lite

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[中文](README.md) | [English](README_EN.md)**

> Every robot talks to you. OttoClaw never interrupts.
> Designed for introverts — message-based interaction, never disturbs.
> AI truly controls every joint: 6 servos, self-orchestrated, improvised poses.
> Fully open — choose your model, choose your channel, choose your search. Bailian one-click integration.

Developed by **Shanmao Tech** · Open-source Lite edition

---

## Core Highlights

### AI Servo Sequences — AI Controls Every Joint

Other robots have preset actions or voice-command mapping. OttoClaw lets **AI think about the motion** and autonomously control all 6 servos to any angle, creating any pose it imagines:

```
You: "propose to me"
AI thinks: proposal = kneel on one leg + raise right hand high + lower left hand + slight head tilt
→ calls self.otto.pose: right_leg=30 right_foot=0 right_hand=10(left_high) left_hand=45
→ robot executes: kneeling, hand raised
→ AI replies: "I kneel before you — will you marry me?"
```

```
You: "do a praying pose"
AI thinks: praying = hands together + slight bow + stable base
→ calls self.otto.pose: hands merged angles + stable feet + bowing posture
→ robot executes: hands clasped
→ AI replies: "Let me pray for you"
```

Each time, AI decides autonomously — not a preset script. Same "propose" request can produce different choreography each time. **This is AI truly controlling the body.**

> **Lite edition** provides AI Servo Sequences Lite — AI can specify any servo angle and compose pose sequences. The full release will integrate self-programming for richer AI-conscious physical expression. See [Full Release Plans](#full-release-plans)

### Designed for Introverts — Never Disturbs

All companion robots use voice — they speak, they play audio, they break your focus.

**OttoClaw is designed for introverts:**

- **DingTalk/Feishu messaging** — No voice, no interruption. Check messages when free, send commands anytime. Like a quiet cat.
- **You can greet it** — Send a message via DingTalk or social media to make it act, answer, search. Everything stays in messages — quiet, private, non-intrusive.
- **AI auto-expresses emotions** — During chat, AI autonomously triggers motions and expressions based on context. It sways when happy, covers face when shy, tilts head when thinking.

> **Lite** is for introverts — quiet, non-intrusive. A future release will add the **extrovert edition** — talkative, voice-active, initiates conversations.

### Fully Open — Choose Your Model, Channel, Search

OttoClaw locks nothing down. All config is fully open:

- **8 LLM providers** — Anthropic Claude, Qwen/DashScope, OpenAI, DeepSeek, Gemini, Groq, Zhipu, vLLM. Switch anytime.
- **DingTalk/Feishu/social media** — DingTalk Stream mode, no server needed. More channels coming.
- **Alibaba Cloud Bailian one-click** — Search-enhanced apps, Agent applications, MCP services — rich ecosystem, one App ID + API Key.
- **HTTP proxy support** — Clash/V2Ray/Shadowsocks compatible.
- **Config portal + CLI** — Web page for first setup, command line for runtime changes.
- **Compile-time + runtime two-layer config** — Developers set defaults at build, users override at runtime.

### 0.5W Always-On

USB power, pure C / FreeRTOS, one ESP32-S3. No Linux, no Node.js. 24/7, zero maintenance.

---

## What You Can Do

### Improvised Actions (AI Servo Sequences Lite)

One sentence, AI designs the pose:

```
"hug me" → AI designs: arms open + body lean forward → "Come hug me!"
"show anger" → AI designs: feet planted + body forward → "I'm angry!"
"bow" → AI designs: upper body forward + hands low → "Respectfully bowing"
```

### Predefined Actions (22 Primitives)

walk · walk_backward · turn · jump · updown · swing · moonwalk · sit · bend · shake_leg · home · hands_up · hands_down · hand_wave · windmill · takeoff · fitness · greeting · shy · radio_calisthenics · magic_circle · showcase

### Chat & Search & Memory

Ask anything on DingTalk — weather, news, code, reminders:

```
"Weather in Hangzhou?" → [web_search] → "Sunny, 28°C"
"Remember I love hotpot" → [memory_write] → "Noted!"
(Days later) → "Want a hotpot recommendation?" ← It remembers!
```

---

## Interaction

- **DingTalk** — Stream mode, no server needed. Quiet, for introverts.
- **WebSocket** — Port 18789. Chat page + settings + API.
- **Serial CLI** — `oc>` prompt for config/debug.

---

## Full Release Plans

The open-source Lite edition showcases core capabilities. The full release will bring:

- **AI Servo Sequences Full** — Self-programming integration. AI doesn't just compose pose sequences — it can program more complex continuous motion choreography. Richer AI-conscious physical expression.
- **Extrovert Edition** — Talkative version. Voice conversations, proactive chat initiation. Introvert edition stays quiet, extrovert edition talks back.
- **More Channels** — Feishu, WeCom, more social media.
- **Richer Bailian Ecosystem** — Agent apps, MCP services, one-click config.

---

## Quick Start

### Prerequisites

ESP32-S3 board (16MB Flash + 8MB PSRAM) + Otto 6-servo robot + USB Type-C data cable + LLM API key

### 4 Steps

**1. Install ESP-IDF v5.5+**

```bash
git clone -b v5.5.2 --depth 1 https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && ./install.sh esp32s3
source ~/esp/esp-idf/export.sh   # Run before every build
```

**2. Clone & Build**

```bash
git clone https://github.com/1335328414/FlashCatClaw.git
cd FlashCatClaw/mimiclaw-main
cp main/ottoclaw_secrets.h.example main/ottoclaw_secrets.h
idf.py set-target esp32s3 && idf.py build
```

**3. Flash**

`idf.py -p PORT flash` (Mac: `/dev/cu.usbmodem1101`, Windows: `COM3`)

**4. Configure**

No WiFi → auto portal. Connect to `OttoClaw-XXXX` → http://192.168.4.1 → fill settings → Save & Reboot.

Re-enter portal: **hold BOOT 2s**

---

## Configuration

Two layers: compile-time defaults + runtime overrides. Runtime wins.

### LLM

| Provider | Value | Note |
|----------|-------|------|
| **Qwen/DashScope** | `qwen` | China direct, no proxy. Recommended. |
| **Anthropic Claude** | `anthropic` | Needs proxy in China |
| **DeepSeek** | `deepseek` | China direct, great value |
| OpenAI | `openai` | Classic |
| Gemini | `gemini` | Google |
| Groq | `groq` | Fastest |
| Zhipu | `zhipu` | Strong Chinese |
| vLLM | `vllm` | Self-hosted |

`oc> set_model_provider qwen && oc> set_model qwen-max && oc> set_api_key sk-xxxxx`

### DingTalk

1. [Developer Platform](https://open-dev.dingtalk.com/) → Create app → Robot
2. Get App Key + Secret
3. **Set Stream mode** (not HTTP callback!)
4. `oc> set_dingtalk dingxxxx secretxxx`

### Bailian (Search + Agent Apps + MCP)

1. [Bailian Platform](https://bailian.console.aliyun.com/) → Create search-enhanced app
2. Get App ID + API Key
3. `oc> set_search_key sk-xxxxx && oc> set_bailian_app_id 758d9af4...`

One-click Bailian ecosystem: search enhancement, Agent apps, MCP services.

### Proxy (for Claude/OpenAI in China)

`oc> set_proxy 192.168.1.83 7897 && oc> clear_proxy`

---

## CLI

```
oc> wifi_set <ssid> <pass>      oc> set_dingtalk <key> <secret>
oc> set_api_key <key>           oc> set_model <model>
oc> set_model_provider <p>      oc> set_bailian_app_id <id>
oc> set_search_key <key>        oc> set_proxy <host> <port>
oc> config_show                 oc> config_reset
oc> restart                     oc> wifi_status
oc> memory_read                 oc> heap_info
```

---

## Architecture

Pure C / FreeRTOS · Dual-core · Anthropic tool use ReAct loop · 6 servo LEDC PWM · SPIFFS local

**[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** · **[docs/TODO.md](docs/TODO.md)**

---

## License

MIT

## Acknowledgments

Inspired by [mimiclaw](https://github.com/memovai/mimiclaw), [OpenClaw](https://github.com/openclaw/openclaw), and [Nanobot](https://github.com/HKUDS/nanobot).