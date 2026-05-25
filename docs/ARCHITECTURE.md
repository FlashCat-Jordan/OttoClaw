# OttoClaw Architecture

> ESP32-S3 AI Agent firmware in pure C / FreeRTOS, centered on DingTalk, WebSocket, Config Portal, local memory, and embodied device feedback.

---

## System Overview

```
DingTalk App / Web Client / Serial CLI
        │
        │  DingTalk HTTP polling / WebSocket / USB serial
        ▼
┌────────────────────────────────────────────────────────────┐
│                    ESP32-S3 (OttoClaw)                │
│                                                            │
│  ┌──────────────┐   ┌──────────────┐   ┌────────────────┐  │
│  │ DingTalk Bot │──▶│              │   │ Config Portal  │  │
│  │   (Core 0)   │   │ Inbound Bus  │   │ AP + Web UI    │  │
│  └──────────────┘   │   Queue      │   │                │  │
│                     └──────┬───────┘   └────────────────┘  │
│  ┌──────────────┐          │                               │
│  │ WebSocket    │──────────┘                               │
│  │ Gateway      │                                          │
│  │ (:18789)     │                                          │
│  └──────────────┘                                          │
│                               ┌──────────────────────────┐ │
│                               │ Agent Loop (Core 1)      │ │
│                               │ - context_builder        │ │
│                               │ - llm_proxy              │ │
│                               │ - tool_registry          │ │
│                               │ - session + memory       │ │
│                               └──────────┬───────────────┘ │
│                                          │                 │
│                                    Outbound Queue          │
│                                          │                 │
│                               ┌──────────▼───────────────┐ │
│                               │ Outbound Dispatch        │ │
│                               │ - DingTalk send          │ │
│                               │ - WebSocket send         │ │
│                               └──────────────────────────┘ │
│                                                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Local Device Services                               │  │
│  │ - LCD mood / chat display                           │  │
│  │ - Otto motion feedback                              │  │
│  │ - Cron / Heartbeat services                         │  │
│  │ - Voice transcription module                        │  │
│  │ - Serial CLI                                        │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ SPIFFS                                               │  │
│  │ /spiffs/config   bootstrap markdown files            │  │
│  │ /spiffs/memory   long-term memory + daily notes      │  │
│  │ /spiffs/sessions per-chat JSONL session history      │  │
│  │ /spiffs/skills   optional skill markdown files       │  │
│  └──────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────┘
                 │
                 │ HTTPS
                 ▼
        Claude-compatible LLM API + Brave Search API
```

---

## Runtime Modes

### 1. Config Portal Mode
The device enters config portal mode when either of these is true:
- BOOT button is held during startup
- no saved WiFi credentials are found

In this mode, `config_portal_start()` starts AP mode plus a local web UI for configuring:
- WiFi
- LLM provider / model / API key / base URL
- DingTalk app key + secret
- proxy host / port
- search key
- Otto test actions

The portal owns the flow and restarts the device when configuration is complete.

### 2. Normal Chat Mode
If WiFi credentials exist and startup does not force portal mode:
- the device connects to WiFi
- LCD transitions through connecting / connected states
- DingTalk, WebSocket, cron, heartbeat, and agent loop services start
- outbound replies are routed back to the originating channel

---

## Data Flow

```
1. A message arrives from DingTalk or WebSocket.
2. The channel module normalizes it into ottoclaw_msg_t.
3. The message is pushed into the inbound FreeRTOS queue.
4. agent_loop.c pops the message and builds context:
   - bootstrap files from SPIFFS
   - long-term memory
   - recent daily notes
   - loaded skills
   - session history
5. llm_proxy.c calls the configured LLM with tool definitions.
6. If tool_use is returned:
   - tool_registry executes the requested tool(s)
   - tool results are appended to the conversation
   - the loop continues until final text is produced
7. The final assistant text is:
   - saved into session history
   - shown on the LCD with mood updates
   - optionally mapped to Otto feedback actions
   - pushed to the outbound queue
8. outbound_dispatch_task() routes the reply to DingTalk or WebSocket.
```

---

## Module Map

```
main/
├── ottoclaw.c                  App entry, startup orchestration, service lifecycle
├── ottoclaw_config.h           Global constants, SPIFFS paths, NVS keys, defaults
├── ottoclaw_secrets.h          Optional build-time defaults (gitignored)
├── ottoclaw_secrets.h.example  Template for build-time defaults
│
├── bus/
│   ├── message_bus.h       ottoclaw_msg_t and queue API
│   └── message_bus.c       inbound/outbound FreeRTOS queues
│
├── wifi/
│   ├── wifi_manager.h
│   └── wifi_manager.c      WiFi STA lifecycle + saved credential handling
│
├── dingtalk/
│   ├── dingtalk_bot.h
│   └── dingtalk_bot.c      polling + message send path
│
├── gateway/
│   ├── ws_server.h
│   └── ws_server.c         WebSocket server + local web entry points
│
├── config_portal/
│   └── config_portal.c     AP mode portal and JSON config APIs
│
├── llm/
│   ├── llm_proxy.h
│   └── llm_proxy.c         LLM provider calls and tool_use parsing
│
├── agent/
│   ├── agent_loop.h
│   ├── agent_loop.c        main ReAct loop
│   ├── context_builder.h
│   └── context_builder.c   system prompt assembly from local files
│
├── tools/
│   ├── tool_registry.c     tool registration and dispatch
│   ├── tool_web_search.c   Brave Search tool
│   ├── tool_get_time.c     time tool
│   ├── tool_files.c        SPIFFS file tools
│   └── tool_otto.c         Otto action tool
│
├── memory/
│   ├── memory_store.c      MEMORY.md + daily memory files
│   └── session_mgr.c       per-chat JSONL sessions
│
├── skills/
│   └── skills.c            loads markdown skills from /spiffs/skills
│
├── lcd/
│   └── lcd_display.c       LCD expressions and chat rendering
│
├── otto/
│   └── otto_movements.c    robot motion primitives
│
├── cron/
│   └── cron_service.c      scheduled background service
│
├── heartbeat/
│   └── heartbeat_service.c periodic background service
│
├── voice/
│   └── voice_transcription.c  voice module (present, not fully wired into main chat path)
│
├── cli/
│   └── serial_cli.c        serial maintenance/config shell
│
└── proxy/
    └── http_proxy.c        HTTP CONNECT tunnel support
```

---

## FreeRTOS Task Layout

| Task | Core | Purpose |
|------|------|---------|
| `agent_loop` | 1 | LLM call, tool execution, context/session handling |
| `outbound` | 0 | Route assistant replies back to channels |
| DingTalk polling task | 0 | Receive messages from DingTalk |
| `serial_cli` | 0 | Local serial REPL |
| HTTP server internals | 0 | WebSocket + local HTTP handling |
| WiFi/IDF event tasks | IDF-managed | connectivity lifecycle |

Core split is intentionally simple: Core 0 handles I/O-heavy work, while Core 1 is reserved for the agent loop.

---

## Storage Layout

SPIFFS is used as the main local persistence layer.

### Bootstrap / prompt files
- `/spiffs/config/IDENTITY.md`
- `/spiffs/config/AGENTS.md`
- `/spiffs/config/TOOLS.md`
- `/spiffs/config/SOUL.md`
- `/spiffs/config/USER.md`

### Memory files
- `/spiffs/memory/MEMORY.md`
- `/spiffs/memory/<YYYY-MM-DD>.md`

### Session files
- `/spiffs/sessions/<chat_id>.jsonl`

### Skill files
- `/spiffs/skills/*.md`

Session files store alternating user/assistant entries as JSONL records.

---

## Configuration Model

Current configuration is **two-layered**:

1. **Build-time defaults** from `ottoclaw_secrets.h`
2. **Runtime overrides** stored in NVS and edited via CLI or Config Portal

This is the current model used by the firmware; configuration is not build-time-only.

### Main runtime config domains
- WiFi
- DingTalk credentials
- LLM API key / provider / model / base URL
- proxy host / port
- search key
- whisper base URL

### User-facing config surfaces
- **Config Portal** for first-time setup and mobile configuration
- **Serial CLI** for maintenance and direct edits
- **Build-time secrets** for default fallback values

---

## Tool Use

The agent loop uses Anthropic-style tool use with a local tool registry.

Current built-in tools include:
- `web_search`
- `get_current_time`
- `read_file`
- `write_file`
- `edit_file`
- `list_dir`
- `memory_write`
- `memory_append_today`
- `self.otto.action` (when motion is enabled)

Tools are registered in `main/tools/tool_registry.c` and exposed to the LLM as JSON schema definitions.

---

## Device Feedback Layer

The fork adds embodied feedback that is central to this project variant:
- LCD mood / state rendering
- streamed chat text on the screen
- mood-derived Otto actions
- portal-triggered motion tests

This embodied device layer is a deliberate local extension and should be treated as part of the main architecture, not as a side demo.

---

## Voice / Cron / Heartbeat Status

These modules already exist in the firmware and are initialized at startup:
- `voice_transcription_init()`
- `cron_service_init()` / `cron_service_start()`
- `heartbeat_service_init()` / `heartbeat_service_start()`

However, voice is not yet fully integrated as a first-class inbound chat channel. That remains a future optimization item rather than a current core flow.

---

## OTA Status

OTA is **not part of the current recommended product surface for this fork**.

Even if OTA-related code or partition history still exists in parts of the repository, the current branch direction is:
- do not treat OTA as a primary capability
- do not document it as part of the main user flow
- only keep or remove remaining OTA code based on whether it is still referenced by the build/runtime

---

## Key Source References

- Startup orchestration: `main/ottoclaw.c`
- Agent loop: `main/agent/agent_loop.c`
- Context assembly: `main/agent/context_builder.c`
- Tool registry: `main/tools/tool_registry.c`
- DingTalk channel: `main/dingtalk/dingtalk_bot.c`
- WebSocket gateway: `main/gateway/ws_server.c`
- Config portal: `main/config_portal/config_portal.c`
- Skills loading: `main/skills/skills.c`
- Runtime config constants: `main/ottoclaw_config.h`
