# OttoClaw Optimization Roadmap

> This backlog tracks the next useful optimization work for the current DingTalk + WebSocket + Config Portal fork.
> It is **not** a Telegram parity checklist and does **not** treat OTA as an active goal.

---

## P0 — Current Surface Alignment

### [ ] Finish runtime/documentation alignment
- Make sure `README.md`, `README_EN.md`, `docs/ARCHITECTURE.md`, and web/CLI help all describe the same runtime model.
- Keep the public story centered on DingTalk, WebSocket, Config Portal, LCD, Otto, and local memory.

### [ ] Remove or clearly downgrade stale Telegram/OTA residue
- Delete or de-emphasize references that imply Telegram or OTA are current first-class product features.
- Keep only intentional historical mentions or explicit “deferred / out of scope” notes.

### [ ] Unify configuration surfaces
- Ensure Config Portal, Serial CLI, README examples, and NVS keys all use the same naming and same config expectations.
- Focus on WiFi, LLM, DingTalk, proxy, search key, and whisper base URL.

### [ ] Audit build-only leftovers
- Confirm whether `main/ota/ota_manager.c` and OTA-related dependencies are still referenced.
- Remove them from the build only if they are truly unused.

---

## P1 — High-Value Functional Improvements

### [ ] Wire voice into the main inbound chat path
- `voice_transcription_init()` already exists, but voice is not yet a full first-class chat input.
- Goal: audio input → transcription → `message_bus` inbound queue → normal agent flow.

### [ ] Improve WebSocket gateway protocol
- Add richer message/event types.
- Consider token streaming events such as `{"type":"token"}` for a better live client experience.
- Keep gateway protocol simple and compatible with future phone/web bridge layers.

### [ ] Strengthen startup/service gating
- Make optional services more clearly conditional where appropriate.
- Reduce unnecessary initialization work for disabled or unused features.
- Keep `main/ottoclaw.c` easier to reason about as the firmware grows.

### [ ] Continue prompt / memory / skills refinement
- Build on existing `context_builder.c` and `skills.c`.
- Improve how bootstrap files, long-term memory, recent notes, and loaded skills are combined.
- Keep the implementation lightweight and SPIFFS-based.

### [ ] Tighten Web/CLI maintenance parity
- Ensure operations available in CLI have sensible equivalents in the portal where that helps real usage.
- Useful targets: config inspection, memory inspection, session inspection, runtime status.

---

## P2 — Follow-on Expansion

### [ ] Mobile bridge layer on top of WebSocket
- Provide a cleaner path for future phone or chat-app bridge services without changing the agent core.
- Reuse the existing channel + message bus architecture.

### [ ] More structured outbound/inbound metadata
- Extend `ottoclaw_msg_t` only if needed by future media, bridge, or richer session features.
- Avoid premature complexity.

### [ ] Session metadata improvements
- Add richer timestamps or session headers only if they clearly improve operations/debugging.

### [ ] Additional channel adapters
- If future channels are added, keep them as adapters into the same message bus / outbound dispatch model.
- No Telegram restoration is planned in the current roadmap.

---

## Explicitly Out of Scope for This Roadmap

### Telegram restoration
- The current fork uses DingTalk as the main mobile chat path.
- Telegram-specific parity work is not the current target.

### OTA implementation
- OTA is not being pursued in the current phase, especially not if it requires extra service/backend support.
- If revisited later, it should be treated as a separate project with its own distribution strategy.

---

## Recommended Execution Order

1. Surface alignment and stale-feature cleanup
2. OTA residue audit and safe build cleanup
3. Voice main-path integration
4. WebSocket protocol enhancement
5. Startup/service gating cleanup
6. Prompt/skills/memory refinement
7. Future bridge/channel expansion
