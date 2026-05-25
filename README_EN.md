# OttoClaw Lite

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[中文](README.md) | [English](README_EN.md)**

> Every robot talks to you. OttoClaw never interrupts.
> It quietly messages you through DingTalk — greet it, command it, make it dance.
> AI truly controls the body: 6 servos, 22 actions, self-orchestrated.

Developed by **Shanmao Tech** · Open-source Lite edition

---

## What Makes It Different

All companion robots use voice — they speak, they play audio, they break your focus.

**OttoClaw takes a completely different path:**

- **Non-intrusive** — Interacts through DingTalk / Feishu messages. Quiet like a cat. No sudden voice during your meeting. Check messages when free, send commands anytime.
- **AI is the soul, not a remote** — Not just "wave when I say wave". The LLM decides autonomously what motion, expression, and words to use. Say "dance for me" — AI choreographs: walk → wave → moonwalk → calisthenics → windmill → finish. Every routine is AI's own composition, not your script.
- **Self-orchestrated actions** — 22 primitives, AI chains them freely via ReAct loop. Same "dance" request can produce different choreography each time. True AI body control.
- **6-servo full body** — Hands + legs + feet = 6 independent joints. Walk, turn, jump, sit, bend, wave, moonwalk, calisthenics, windmill, shy, fitness... each tunable: steps, speed, direction, amount.
- **0.5W always-on** — USB power, pure C / FreeRTOS, one ESP32-S3. 24/7, zero maintenance.

---

## Play With It

### Chat

Message it on DingTalk — anything goes. Weather, news, code, stories, reminders. It searches the web, reads/writes files, remembers things.

```
You: "What's the weather in Hangzhou today?"
OttoClaw: [web_search] → "Hangzhou: sunny, 28°C — perfect for a walk out"
```

### Make It Dance

One sentence, AI choreographs the whole routine:

```
You: "dance for me"
OttoClaw: → walk 3 steps → moonwalk → radio_calisthenics → swing → showcase → "Done dancing!"
```

```
You: "be shy"
OttoClaw: → shy (head down + cover face + tiny tremble) → "Ohhh, so embarrassed~"
```

```
You: "do some fitness"
OttoClaw: → sit → fitness (sit down + raise arms + stretch) → "Fitness time!"
```

### Make It Perform

Full routines, one call:

- **Radio calisthenics** — 8 full sections, arms+legs+side+bend, better form than you
- **Showcase** — walk → wave → calisthenics → moonwalk → swing → takeoff → fitness → backward → grand finale
- **Magic circle** — 40 seconds of spinning joy, all 6 joints rotating together
- **Windmill** — both arms spinning like a windmill
- **Takeoff** — arms flapping up and down, simulating flight

### Live With It

OttoClaw remembers. Your preferences, your stories, your daily events. Across restarts:

```
You: "Remember I love hotpot"
OttoClaw: [memory_write] → "Noted! Will suggest hotpot places next time"

You: "I passed my interview today!"
OttoClaw: [memory_append_today] → "Congrats! Logged in today's notes"

(Days later)
OttoClaw: "Your interview was last week — how's the new job going?"
```

---

## Action List (Lite · 22 Primitives)

| Category | Actions | Params |
|----------|---------|--------|
| **Walking** | walk · walk_backward · turn | steps, speed, direction, amount |
| **Jumping** | jump · updown | steps, speed, amount |
| **Grooving** | swing · moonwalk | steps, speed, amount, direction |
| **Poses** | sit · bend · shake_leg · home | steps, speed, direction |
| **Hands** | hands_up · hands_down · hand_wave | speed, direction |
| **Flashy** | windmill · takeoff · fitness | steps, speed, amount |
| **Emotions** | greeting · shy | direction, steps |
| **Routines** | radio_calisthenics · magic_circle · showcase | — |

> **Lite note:** Open-source Lite edition provides 22 predefined action primitives. AI orchestrates sequences via ReAct loop. The low-level arbitrary servo position control (`otto_move_servos`) already exists — full Servo Sequences tool interface (AI-defined custom angle + timeline choreography) will ship in the official release, with self-programming capability for more AI-conscious physical expression.

---

## Interaction

- **DingTalk** — Quiet companion. Stream mode, no server needed. Set App Key + Secret, done.
- **WebSocket** — Port 18789. Chat at `http://<ip>:18789`, settings at `/settings.html`
- **Serial CLI** — `oc>` prompt via USB for config/debug

---

## Quick Start

```bash
# ESP-IDF v5.5+ first: https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/
git clone https://github.com/1335328414/FlashCatClaw.git
cd FlashCatClaw/mimiclaw-main
cp main/ottoclaw_secrets.h.example main/ottoclaw_secrets.h
idf.py set-target esp32s3 && idf.py build && idf.py -p PORT flash
```

No WiFi → auto portal mode. Connect to `OttoClaw-XXXX`, open `http://192.168.4.1`. Configure everything in the web UI. Hold **BOOT 2s** to re-enter.

---

## Config

Two layers: compile-time defaults (`ottoclaw_secrets.h`) + runtime overrides (NVS, portal/CLI). Runtime wins.

### LLM

| Provider | Value | One-liner |
|----------|-------|-----------|
| Anthropic (Claude) | `anthropic` | Default, world-class |
| Qwen (DashScope) | `qwen` | Direct connect in China, no proxy |
| OpenAI (GPT) | `openai` | Classic |
| DeepSeek | `deepseek` | Best value |
| Gemini | `gemini` | Google |
| Groq | `groq` | Speed ceiling |
| Zhipu (GLM) | `zhipu` | Strong Chinese |
| vLLM | `vllm` | Self-hosted, needs base_url |

```
oc> set_model_provider qwen && oc> set_model qwen-max && oc> set_api_key sk-xxxxx
```

### DingTalk

[Open platform](https://open-dev.dingtalk.com/) → create app → robot → get App Key + Secret → enable Stream mode.

`oc> set_dingtalk dingxxxx secretxxx`

### Search

[Bailian platform](https://bailian.console.aliyun.com/) → create search-enhanced app → get App ID + API Key.

`oc> set_search_key sk-xxxxx && oc> set_bailian_app_id 758d9af4...`

### Proxy (China users)

`oc> set_proxy 192.168.1.83 7897 && oc> clear_proxy` (Clash/V2Ray HTTP port)

---

## CLI

```
oc> wifi_set <ssid> <pass>      oc> set_dingtalk <key> <secret>
oc> set_api_key <key>           oc> set_model <model>
oc> set_model_provider <p>      oc> set_bailian_app_id <id>
oc> set_search_key <key>        oc> set_proxy <host> <port>
oc> config_show                 oc> config_reset
oc> memory_read                 oc> memory_write "content"
oc> heap_info                   oc> restart
```

---

## Memory

All data stored as plain text on SPIFFS. AI reads/writes:

- `SOUL.md` — Who it is, personality
- `USER.md` — What it knows about you
- `MEMORY.md` — Long-term memory (cross-session)
- `YYYY-MM-DD.md` — Daily auto-notes
- `<chat_id>.jsonl` — Chat history

Say "remember I love hotpot" → writes to `MEMORY.md` → later asks "hotpot place nearby?" That's companionship.

---

## Architecture

- **Pure C / FreeRTOS** — No Linux, no Python, one ESP32-S3
- **Dual-core** — Core 0 network, Core 1 agent
- **Anthropic-style tool use** — ReAct loop, AI decides tool calls
- **SPIFFS local** — Memory, sessions, config, skills — all on device
- **6 servo LEDC PWM** — Independent joint control, oscillator-driven smooth motion

**[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** · **[docs/TODO.md](docs/TODO.md)**

---

## License

MIT

## Acknowledgments

Inspired by [mimiclaw](https://github.com/memovai/mimiclaw), [OpenClaw](https://github.com/openclaw/openclaw), and [Nanobot](https://github.com/HKUDS/nanobot). We took the AI Agent architecture to embedded hardware and made it an embodied, playful device experience.