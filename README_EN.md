# OttoClaw Lite

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[中文](README.md) | [English](README_EN.md)**

> While every robot tries to talk to you, OttoClaw chooses silence.
> Designed for introverts — message-based interaction, never disturbs.
> AI autonomously controls every joint: 6 servos, improvised poses, free composition.
> Fully open architecture — choose your model, channel, and search. Bailian one-click integration.

Developed by **Shanmao Tech** · Open-source Lite edition

---

## Core Highlights

### AI Servo Sequences — AI Autonomously Controls Every Joint

Existing robots rely on preset actions or voice-command mapping. OttoClaw grants AI the ability to **autonomously reason about motion**: the LLM interprets semantic intent and independently decides which angles each of the 6 servos should reach, creating any pose it can imagine.

```
User: "propose to me"
AI reasoning: proposal = kneel on one leg + raise right hand high + lower left hand + slight tilt
→ calls self.otto.pose: right_leg=30° right_foot=0° right_hand=10°(raised) left_hand=45°(lowered)
→ Robot executes: kneeling, hand raised
→ AI replies: "I kneel before you — will you marry me?"
```

```
User: "do a praying pose"
AI reasoning: praying = hands together + slight bow + stable stance
→ calls self.otto.pose: hands merged angles + stable feet + bowing posture
→ Robot executes: hands clasped
→ AI replies: "Let me pray for you"
```

All poses are autonomously reasoned and composed by AI, not preset scripts. The same "propose" request may yield different choreography each time. **This is AI truly controlling the body.**

> Lite edition provides AI Servo Sequences Lite — AI can specify any servo angle and compose pose sequences. The full release will integrate self-programming for richer AI-conscious physical expression. See [Full Release Plans](#full-release-plans)

### Designed for Introverts — Never Disturbs

Current companion robots universally adopt voice interaction — speaking, playing audio, interrupting focus.

**OttoClaw takes a fundamentally different interaction philosophy, designed for introverts:**

- **DingTalk/Feishu messaging** — No voice, no interruption. When busy, it waits quietly; when available, a glance at messages and a quick command triggers response. Always present, never intrusive.
- **Bidirectional interaction** — Users can send messages via DingTalk or social media to trigger actions, get answers, or search information. All interaction remains in the message layer — quiet, private, non-intrusive.
- **Autonomous emotional expression** — During conversation, AI triggers motions and expressions based on context without user instruction. Swaying when happy, covering face when shy, tilting head when thinking — emotional expression is spontaneous, not passive.

> Lite edition is for introverts — quiet, non-intrusive. A future release will offer an **extrovert edition** with voice conversation capability and proactive chat initiation.

### Fully Open Architecture — Choose Your Model, Channel, Search

OttoClaw locks nothing down. All configuration is fully open:

- **8 LLM providers** — Anthropic Claude, Qwen/DashScope, OpenAI, DeepSeek, Gemini, Groq, Zhipu, vLLM. Free choice, switch anytime.
- **DingTalk/Feishu/social media** — DingTalk Stream mode, no server needed. Feishu, WeCom, and more channels continuously expanding.
- **Alibaba Cloud Bailian one-click** — Search enhancement, Agent apps, MCP services — one App ID + API Key connects to Bailian's rich ecosystem.
- **HTTP proxy support** — Clash/V2Ray/Shadowsocks compatible. Works under restricted network conditions.
- **Config portal + CLI dual entry** — Web page for initial setup, command line for runtime adjustments.
- **Compile-time + runtime two-layer config** — Developer defaults at build time, user overrides at runtime, no conflict.

### 0.5W Always-On

USB powered, pure C / FreeRTOS, single ESP32-S3. No Linux, no Node.js, no bloated dependencies. 24/7 online at 0.5W, zero maintenance required.

### Fully Open-Source Hardware

This firmware runs on the **Shanmao official dev board**. Hardware is also open-source, or purchase directly:

- **Official dev board — one-click purchase** — [Shanmao Robot Official Store](https://m.tb.cn/h.SRXKaIT7OtBRrpQ)
- **DIY kit (all components included, pre-flashed, 40 min assembly)** — [Shanmao Robot DIY Kit](https://e.tb.cn/h.SRfKOWrlDXV4kQR?tk=atRsf1poxdZ)
- **PCB + BOM open-source files** — Self-manufacturing also possible: [LCSC Open Hardware](https://oshwhub.com/txp666/ottorobot)
- **3D-printed shell STL files** — [MakerWorld @shanmaotech](https://makerworld.com.cn/@shanmaotech)
- **Complete assembly & usage guide** — [shanmaotech.cn/ottodiy](https://www.shanmaotech.cn/ottodiy/)

---

## Capabilities

### Improvised Action Composition (AI Servo Sequences Lite)

A single sentence, AI handles the entire process from semantic understanding to pose design:

```
"Give me a hug pose" → AI reasons pose → arms open + lean forward → "Come hug me!"
"Show anger" → AI reasons pose → feet planted + body forward → "I'm angry!"
"Bow" → AI reasons pose → upper body forward + hands low → "Respectfully bowing"
```

### Predefined Actions (22 Primitives)

| Walking | walk / walk_backward / turn |
| Jumping | jump / updown |
| Grooving | swing / moonwalk |
| Poses | sit / bend / shake_leg / home |
| Hands | hands_up / hands_down / hand_wave |
| Flashy | windmill / takeoff / fitness |
| Emotions | greeting / shy |
| Routines | radio_calisthenics / magic_circle / showcase |

### Conversation, Search, and Memory

Interact via DingTalk — conversation, web search, and long-term memory:

```
"How's the weather in Hangzhou?" → [web_search] → "Sunny, 28°C, nice for a walk"
"Remember I love hotpot" → [memory_write] → "Recorded"
(Days later) → "Want a hotpot recommendation?" ← Remembers across reboots
```

---

## Interaction Channels

- **DingTalk** — Primary chat channel. Stream mode, no server needed. Quiet, designed for introverts.
- **WebSocket** — Port 18789. Built-in chat page, settings page, and WebSocket API for developer integration.
- **Serial CLI** — `oc>` prompt for local maintenance and configuration.

---

## Full Release Plans

The open-source Lite edition showcases core capabilities. The full release will deliver:

- **AI Servo Sequences Full** — Self-programming integration. AI not only composes pose sequences but can program more complex continuous motion choreography. Richer AI-conscious physical expression.
- **Extrovert Edition** — Voice conversation capability, proactive chat initiation. Introvert edition stays quiet; extrovert edition is talkative and engaging.
- **More Channels** — Feishu, WeCom, and additional social media integrations.
- **Deeper Bailian Integration** — Agent apps, MCP services, one-click configuration.

---

## Quick Start (Beginner Guide)

### Prerequisites

1. ESP32-S3 board (16MB Flash + 8MB PSRAM) + Otto 6-servo robot kit
2. USB Type-C data cable (must support data transfer)
3. LLM API Key (see Configuration Guide below)
4. Optional: DingTalk account, Alibaba Cloud account

### Step 1: Install Build Tools

```bash
# Mac / Linux
git clone -b v5.5.2 --depth 1 https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && ./install.sh esp32s3
source ~/esp/esp-idf/export.sh   # Execute before each build session
```

Windows: [ESP-IDF Windows Setup Guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/windows-setup.html)

Verify: run `idf.py` in terminal — help output means success.

### Step 2: Get the Code

```bash
git clone https://github.com/1335328414/FlashCatClaw.git
cd FlashCatClaw/mimiclaw-main
cp main/ottoclaw_secrets.h.example main/ottoclaw_secrets.h
```

### Step 3: Build & Flash

```bash
idf.py set-target esp32s3 && idf.py build
idf.py -p PORT flash
```

PORT: `/dev/cu.usbmodem1101` (Mac) or `COM3` (Windows)

### Step 4: First-Time Network Setup

After flashing, the device enters config portal mode automatically:

1. Connect phone to `OttoClaw-XXXX` hotspot (no password)
2. Open http://192.168.4.1 in browser
3. Fill in settings → Save & Reboot

To re-enter portal: **hold BOOT button for 2 seconds** during power-on.

---

## Configuration Guide

Two-layer config: compile-time defaults (`ottoclaw_secrets.h`) + runtime overrides (NFS/portal/CLI). Runtime values take precedence.

### WiFi

**Portal:** Enter SSID + password → Save & Reboot.

**CLI:** `oc> wifi_set MySSID MyPassword && oc> restart`

Auto-fallback to portal on connection failure — misconfiguration is never permanent.

### LLM

Three items: provider, model name, API key.

**Qwen/DashScope (recommended for China, no proxy):**
1. [DashScope Console](https://dashscope.console.aliyun.com/) → Enable service → Create API key
2. `oc> set_model_provider qwen && oc> set_model qwen-max && oc> set_api_key sk-xxxxx`

**Anthropic Claude (proxy required in China):**
1. [Anthropic Console](https://console.anthropic.com/) → Create API key
2. `oc> set_model_provider anthropic && oc> set_model claude-sonnet-4-5 && oc> set_api_key sk-ant-api03-xxxxx`

**DeepSeek (China direct, high value):**
1. [DeepSeek Platform](https://platform.deepseek.com/) → Create API key
2. `oc> set_model_provider deepseek && oc> set_model deepseek-chat && oc> set_api_key sk-xxxxx`

| Other | Value | Note |
|-------|-------|------|
| OpenAI | `openai` | Classic, needs proxy |
| Gemini | `gemini` | Google, needs proxy |
| Groq | `groq` | Extremely fast inference |
| Zhipu | `zhipu` | Strong Chinese NLP |
| vLLM | `vllm` | Self-hosted model service |

### DingTalk

1. [Developer Platform](https://open-dev.dingtalk.com/) → Create app → Robot type
2. Obtain App Key + App Secret
3. **Important**: Set message receiving mode to **Stream** (not HTTP callback)
4. `oc> set_dingtalk dingxxxx secretxxx`

Stream mode establishes a direct WebSocket connection to DingTalk servers — no public server or callback URL required.

### Bailian (Search + Agent Apps + MCP)

1. [Bailian Platform](https://bailian.console.aliyun.com/) → Create app → Search-enhanced type
2. Obtain App ID + API Key
3. `oc> set_search_key sk-xxxxx && oc> set_bailian_app_id 758d9af4...`

One App ID + API Key connects to Bailian ecosystem: search enhancement, Agent apps, MCP services.

### HTTP Proxy

China users accessing Claude/OpenAI may need proxy configuration:
```
oc> set_proxy 192.168.1.83 7897    # Clash/V2Ray HTTP port
oc> clear_proxy                     # Remove proxy
```

### Re-entering Config Portal

- Hold **BOOT button for 2 seconds** during power-on → enters portal mode
- CLI: `oc> config_reset && oc> restart`

---

## CLI Commands

USB serial (115200 baud), `oc>` prompt:

```
oc> wifi_set <ssid> <pass>        Set WiFi credentials
oc> set_dingtalk <key> <secret>   Set DingTalk credentials
oc> set_api_key <key>             Set LLM API key
oc> set_model <model>             Set model name
oc> set_model_provider <provider> Set provider
oc> set_search_key <key>          Set search API key
oc> set_bailian_app_id <id>       Set Bailian App ID
oc> set_proxy <host> <port>       Set HTTP proxy
oc> clear_proxy                   Remove proxy
oc> config_show                   Display current config
oc> config_reset                  Clear runtime config
oc> restart                       Restart device
oc> wifi_status                   Show WiFi status and IP
oc> wifi_scan                     Scan nearby WiFi
oc> memory_read                   Display long-term memory
oc> memory_write "content"        Write to long-term memory
oc> heap_info                     Show available heap memory
oc> session_list                  List chat sessions
```

---

## Memory System

All data stored as plain text on SPIFFS, readable and writable by AI:

| File | Description |
|------|-------------|
| `SOUL.md` | Robot personality and character |
| `USER.md` | User preference profile |
| `MEMORY.md` | Long-term memory (cross-session) |
| `YYYY-MM-DD.md` | Daily notes (auto-generated) |
| `<chat_id>.jsonl` | Chat history (per-conversation archive) |

---

## Technical Architecture

- **Pure C / FreeRTOS** — Single ESP32-S3 runs everything
- **Dual-core** — Core 0 handles network I/O; Core 1 runs Agent loop
- **Anthropic tool use / ReAct loop** — AI autonomously decides tool calls and composition
- **6 servo LEDC PWM** — Independent joint control, oscillator-driven smooth motion
- **SPIFFS local storage** — Memory, sessions, config, skills — all on-device, cloud-independent

Full details: **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** and **[docs/TODO.md](docs/TODO.md)**

---

## License

MIT

---

## Acknowledgments

Inspired by [mimiclaw](https://github.com/memovai/mimiclaw), [OpenClaw](https://github.com/openclaw/openclaw), and [Nanobot](https://github.com/HKUDS/nanobot). We brought the AI Agent architecture to embedded hardware and gave it a more embodied, playful device experience.