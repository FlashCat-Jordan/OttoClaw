# MimiClaw vs Nanobot — Feature Gap Tracker

> Comparing against `nanobot/` reference implementation. Tracks features MimiClaw has not yet aligned with.
> Priority: P0 = Core missing, P1 = Important enhancement, P2 = Nice to have

---

## P0 — Core Agent Capabilities

### [x] ~~Tool Use Loop (multi-turn agent iteration)~~
- Implemented: `agent_loop.c` ReAct loop with `llm_chat_tools()`, max 10 iterations, non-streaming JSON parsing

### [ ] Memory Write via Tool Use (agent-driven memory persistence)
- **openclaw**: Agent uses standard `write`/`edit` tools to write `MEMORY.md` and `memory/YYYY-MM-DD.md`; system prompt instructs agent to persist important information; pre-compaction memory flush triggers a silent agent turn to save durable memories before context window limit
- **MimiClaw**: `memory_write_long_term` and `memory_append_today` exist but are only called from CLI; agent loop never writes memory
- **Scope**: Expose `memory_write` and `memory_append_today` as tool_use tools for Claude; add system prompt guidance on when to persist memory; optionally add pre-compaction flush (trigger memory save when session history nears `MIMI_SESSION_MAX_MSGS`)
- **Depends on**: Tool Use Loop

### [x] ~~Tool Registry + web_search Tool~~
- Implemented: `tools/tool_registry.c` — tool registration, JSON schema builder, dispatch by name
- Implemented: `tools/tool_web_search.c` — Brave Search API via HTTPS (direct + proxy support)

### [ ] More Built-in Tools
- **nanobot built-in tools** not yet ported: `read_file`, `write_file`, `edit_file`, `list_dir`, `message`
- **Recommendation**: Reasonable tool subset for ESP32: `read_file`, `write_file`, `list_dir` (SPIFFS), `message`, `memory_write`

### [ ] Subagent / Spawn Background Tasks
- **nanobot**: `subagent.py` — SubagentManager spawns independent agent instances with isolated tool sets and system prompts, announces results back to main agent via system channel
- **MimiClaw**: Not implemented
- **Recommendation**: ESP32 memory is limited; simplify to a single background FreeRTOS task for long-running work, inject result into inbound queue on completion

---

## P1 — Important Features

### [ ] Telegram User Allowlist (allow_from)
- **nanobot**: `channels/base.py` L59-82 — `is_allowed()` checks sender_id against allow_list
- **MimiClaw**: No authentication; anyone can message the bot and consume API credits
- **Recommendation**: Store allow_from list in `mimi_secrets.h` as a build-time define, filter in `process_updates()`

### [ ] Telegram Markdown to HTML Conversion
- **nanobot**: `channels/telegram.py` L16-76 — `_markdown_to_telegram_html()` full converter: code blocks, inline code, bold, italic, links, strikethrough, lists
- **MimiClaw**: Uses `parse_mode: Markdown` directly; special characters can cause send failures (has fallback to plain text)
- **Recommendation**: Implement simplified Markdown-to-HTML converter, or switch to `parse_mode: HTML`

### [ ] Telegram /start Command
- **nanobot**: `telegram.py` L183-192 — handles `/start` command, replies with welcome message
- **MimiClaw**: Not handled; /start is sent to Claude as a regular message

### [ ] Telegram Media Handling (photos/voice/files)
- **nanobot**: `telegram.py` L194-289 — handles photo, voice, audio, document; downloads files; transcribes voice
- **MimiClaw**: Only processes `message.text`, ignores all media messages
- **Recommendation**: Images can be base64-encoded for Claude Vision; voice requires Whisper API (extra HTTPS request)

### [ ] Skills System (pluggable capabilities)
- **nanobot**: `agent/skills.py` — loads skills from SKILL.md files, supports always-loaded and on-demand, frontmatter metadata, requirements checking
- **MimiClaw**: Not implemented
- **Recommendation**: Simplified version: store SKILL.md files on SPIFFS, load into system prompt via context_builder

### [ ] Full Bootstrap File Alignment
- **nanobot**: Loads `AGENTS.md`, `SOUL.md`, `USER.md`, `TOOLS.md`, `IDENTITY.md` (5 files)
- **MimiClaw**: Only loads `SOUL.md` and `USER.md`
- **Recommendation**: Add AGENTS.md (behavior guidelines) and TOOLS.md (tool documentation)

### [ ] Longer Memory Lookback
- **nanobot**: `memory.py` L56-80 — `get_recent_memories(days=7)` defaults to 7 days
- **MimiClaw**: `context_builder.c` only reads last 3 days
- **Recommendation**: Make configurable, but mind token budget

### [x] ~~System Prompt Tool Guidance~~
- Implemented: `context_builder.c` includes tool usage guidance in system prompt

### [ ] Message Metadata (media, reply_to, metadata)
- **nanobot**: `bus/events.py` — InboundMessage has media, metadata fields; OutboundMessage has reply_to
- **MimiClaw**: `mimi_msg_t` only has channel + chat_id + content
- **Recommendation**: Extend msg struct, add media_path and metadata fields

### [ ] Outbound Subscription Pattern
- **nanobot**: `bus/queue.py` L41-49 — supports `subscribe_outbound(channel, callback)` subscription model
- **MimiClaw**: Hardcoded if-else dispatch
- **Recommendation**: Current approach is simple and reliable; not worth changing with few channels

---

## P2 — Advanced Features

### [ ] Cron Scheduled Task Service
- **nanobot**: `cron/service.py` — full cron scheduler supporting at/every/cron expressions, persistent storage, timed agent triggers
- **MimiClaw**: Not implemented
- **Recommendation**: Use FreeRTOS timer for simplified version, support "every N minutes" only

### [ ] Heartbeat Service
- **nanobot**: `heartbeat/service.py` — reads HEARTBEAT.md every 30 minutes, triggers agent if tasks are found
- **MimiClaw**: Not implemented
- **Recommendation**: Simple FreeRTOS timer that periodically checks HEARTBEAT.md

### [ ] Multi-LLM Provider Support
- **nanobot**: `providers/litellm_provider.py` — supports OpenRouter, Anthropic, OpenAI, Gemini, DeepSeek, Groq, Zhipu, vLLM via LiteLLM
- **MimiClaw**: Hardcoded to Anthropic Messages API
- **Recommendation**: Abstract LLM interface, support OpenAI-compatible API (most providers are compatible)

### [ ] Voice Transcription
- **nanobot**: `providers/transcription.py` — Groq Whisper API
- **MimiClaw**: Not implemented
- **Recommendation**: Requires extra HTTPS request to Whisper API: download Telegram voice -> forward -> get text

### [x] ~~Build-time Config File + Runtime NVS Override~~
- Implemented: `mimi_secrets.h` as build-time defaults, NVS as runtime override via CLI
- Two-layer config: build-time secrets → NVS fallback, CLI commands to set/show/reset

### [ ] WebSocket Gateway Protocol Enhancement
- **nanobot**: Gateway port 18790 + richer protocol
- **MimiClaw**: Basic JSON protocol, lacks streaming token push
- **Recommendation**: Add `{"type":"token","content":"..."}` streaming push

### [ ] Multi-Channel Manager
- **nanobot**: `channels/manager.py` — unified lifecycle management for multiple channels
- **MimiClaw**: Hardcoded in app_main()
- **Recommendation**: Not worth abstracting with few channels

### [ ] WhatsApp / Feishu Channels
- **nanobot**: `channels/whatsapp.py`, `channels/feishu.py`
- **MimiClaw**: Only Telegram + WebSocket
- **Recommendation**: Low priority, Telegram is sufficient

### [x] ~~Telegram Proxy Support (HTTP CONNECT)~~
- Implemented: HTTP CONNECT tunnel via `proxy/http_proxy.c`, configurable via `mimi_secrets.h` (`MIMI_SECRET_PROXY_HOST`/`MIMI_SECRET_PROXY_PORT`)

### [ ] Session Metadata Persistence
- **nanobot**: `session/manager.py` L136-153 — session file includes metadata line (created_at, updated_at)
- **MimiClaw**: JSONL only stores role/content/ts, no metadata header
- **Recommendation**: Low priority

---

## Completed Alignment

- [x] Telegram Bot long polling (getUpdates)
- [x] Message Bus (inbound/outbound queues)
- [x] Agent Loop with ReAct tool use (multi-turn, max 10 iterations)
- [x] Claude API (Anthropic Messages API, non-streaming, tool_use protocol)
- [x] Tool Registry + web_search tool (Brave Search API)
- [x] Context Builder (system prompt + bootstrap files + memory + tool guidance)
- [x] Memory Store (MEMORY.md + daily notes)
- [x] Session Manager (JSONL per chat_id, ring buffer history)
- [x] WebSocket Gateway (port 18789, JSON protocol)
- [x] Serial CLI (esp_console, debug/maintenance commands)
- [x] HTTP CONNECT Proxy (Telegram + Claude API + Brave Search via proxy tunnel)
- [x] OTA Update
- [x] WiFi Manager (build-time credentials, exponential backoff)
- [x] SPIFFS storage
- [x] Build-time config (`mimi_secrets.h`) + runtime NVS override via CLI

---

## Suggested Implementation Order

```
1. [done] Tool Use Loop + Tool Registry + web_search
2. Memory Write via Tool Use         <- makes the agent actually remember
3. Built-in Tools (read_file, write_file, message)
4. Telegram Allowlist (allow_from)   <- security essential
5. Bootstrap File Completion (AGENTS.md, TOOLS.md)
6. Subagent (simplified)
7. Telegram Markdown -> HTML
8. Media Handling
9. Cron / Heartbeat
10. Other enhancements
```
