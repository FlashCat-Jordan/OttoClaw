# OttoClaw Lite

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[中文](README.md) | [English](README_EN.md)**

> 别的机器人都在跟你说话。OttoClaw 从不打扰你。
> 它悄咪咪给你发钉钉消息 — 你可以问候它、下指令、让它跳舞。
> AI 真正控制身体：6 个舵机，22 个动作，自主编排，像指挥自己的身体一样。

由 **闪猫科技** 研发 · 开源 Lite 版

---

## 不一样在哪

市面上的陪伴机器人全是语音交互 — 说话、播放、打断你。

**OttoClaw 走了一条完全不同的路：**

- **不打扰你** — 通过钉钉、飞书等社媒安静地跟你对话。开会时它不会突然开口，你忙的时候它就安静等着。有空了看一眼消息，回一句指令，它就动了。像养了一只不吵的小猫。
- **AI 是灵魂，不是遥控器** — 不是你说"挥手"它就挥手这么简单。大模型自主决定做什么动作、做什么表情、说什么话。你说一句"跳个舞"，AI 自己编排：先走几步 → 挥手打招呼 → 太空步 → 广播体操 → 风车 → 收尾。每一段都由 AI 自主决定，不是你写的脚本。
- **自编排动作序列** — 22 个动作原语，AI 通过 ReAct 循环自由组合。同样一句"跳个舞"，每次编排可能都不一样。这才是真正的 AI 控制身体。
- **6 舵机全身体** — 双手、双腿、双脚，6 个独立关节。走路、转身、跳跃、坐下、弯腰、挥手、太空步、广播体操、大风车、害羞、健身操……22 个动作每个都可调次数、速度、方向、幅度。
- **0.5W 永不关机** — USB 供电，纯 C / FreeRTOS，一块 ESP32-S3 搞定全部。24/7 在线，不需要你操心。

---

## 看看它能玩什么

### 跟它聊天

在钉钉上给它发消息，它就回复。聊什么都可以 — 问天气、搜新闻、写代码、讲故事、记备忘。它还能联网搜索、读写本地文件、记录长期记忆。

```
你: "今天杭州天气怎么样？"
OttoClaw: [调用 web_search] → "杭州今天晴，28°C，适合出门走走 🌞"
```

### 让它跳舞

你只需要一句话，AI 自己编排整段表演：

```
你: "给我跳一段舞"
OttoClaw: → walk 3步 → moonwalk → radio_calisthenics → swing → showcase → "跳完了！"
```

```
你: "害羞一下"
OttoClaw: → shy (低头+捂脸+小幅度抖动) → "哎呀，好害羞~"
```

```
你: "做健身操"
OttoClaw: → sit → fitness (坐下+举臂+拉伸) → "我做健身操！"
```

### 让它表演

有些动作是完整的套路，AI 一次调用就搞定：

- **广播体操** — 8 节完整操，手臂+腿部+体侧+弯腰，比你做得标准
- **showcase** — 先走几步 → 挥手 → 广播体操 → 太空步 → 摇摆 → 起飞 → 健身操 → 后退 → 完美收场
- **magic_circle** — 40 秒爱的魔力转圈圈，全身 6 个关节一起转
- **大风车** — 双手像风车一样旋转
- **起飞** — 双臂上下摆动，模拟起飞

### 和它生活

OttoClaw 会记忆。它记住你说过的事、你的偏好、每天发生的事。跨重启不会忘：

```
你: "记住我喜欢吃火锅"
OttoClaw: [调用 memory_write] → "记住了！下次推荐火锅店 🍲"

你: "我今天面试通过了"
OttoClaw: [调用 memory_append_today] → "恭喜！记到今天的笔记里了 🎉"

（几天后）
OttoClaw: "上周你面试通过了，新工作适应得怎么样？"
```

---

## 动作清单（Lite 版 · 22 个动作原语）

| 类别 | 动作 | 说明 | 参数 |
|------|------|------|------|
| **行走** | `walk` / `walk_backward` / `turn` | 前走、后退、转弯 | steps, speed, direction, amount |
| **跳跃** | `jump` / `updown` | 跳一下、蹲起 | steps, speed, amount |
| **摇摆** | `swing` / `moonwalk` | 摇摆、太空步 | steps, speed, amount, direction |
| **姿态** | `sit` / `bend` / `shake_leg` / `home` | 坐下、弯腰、摇腿、复位 | steps, speed, direction |
| **双手** | `hands_up` / `hands_down` / `hand_wave` | 举手、放手、挥手 | speed, direction |
| **花式** | `windmill` / `takeoff` / `fitness` | 大风车、起飞、健身操 | steps, speed, amount |
| **情绪** | `greeting` / `shy` | 打招呼、害羞 | direction, steps |
| **套路** | `radio_calisthenics` / `magic_circle` / `showcase` | 广播体操、转圈圈、全套表演 | — |

> **Lite 版说明：** 开源版本提供 22 个预定义动作原语，AI 通过 ReAct 循环自主编排动作序列。完整的 Servo Sequences 能力（任意舵机角度+时间序列的自定义编排）将在后续版本推出。

---

## 交互方式

### 钉钉 — 安静的陪伴

在钉钉上跟 OttoClaw 说话。不需要语音，不需要盯着屏幕。它给你发消息你看一眼，你给它发消息它就行动。开会时它安静等着，下班了你跟它聊两句。

钉钉 Stream 模式直连，不需要公网服务器，不需要回调地址。配好 App Key 和 Secret 就行。

### WebSocket 网关 — 开发者玩法

局域网端口 `18789`：

- `http://<设备IP>:18789` — 浏览器直接聊天
- `ws://<设备IP>:18789/ws` — WebSocket 接口，可以接自己的前端、桥接服务、手机 App
- `http://<设备IP>:18789/settings.html` — 运行时改配置

### 串口 CLI — 本地运维

USB 串口连接，`oc>` 命令行。配 WiFi、改模型、看内存、重启设备。

---

## 快速上手

### 你需要什么

- ESP32-S3 开发板（16MB Flash + 8MB PSRAM）+ Otto 六舵机机器人
- USB Type-C 数据线
- 大模型 API Key（Anthropic / 通义千问 / DeepSeek 等）
- 可选：钉钉机器人凭据 · 百炼搜索 App ID

### 三步搞定

**1. 编译**

```bash
# 先装 ESP-IDF v5.5+: https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/
git clone https://github.com/1335328414/FlashCatClaw.git
cd FlashCatClaw/mimiclaw-main
cp main/ottoclaw_secrets.h.example main/ottoclaw_secrets.h
idf.py set-target esp32s3 && idf.py build
```

**2. 烧录**

```bash
idf.py -p PORT flash
```

**3. 配置**

烧录后没有 WiFi → 自动进入配置门户。手机连 `OttoClaw-XXXX` WiFi → 打开 `http://192.168.4.1` → 填 WiFi + LLM + 钉钉 + 搜索 → 保存并重启。

随时可以重新进门户：**按住 BOOT 键 2秒**。

---

## 配置指南

两层配置机制：编译时默认值（`ottoclaw_secrets.h`）+ 运行时覆盖（NVS，门户/CLI）。运行时优先。

### WiFi

**门户：** 填 WiFi 名称和密码 → 保存重启。

**CLI：** `oc> wifi_set MySSID MyPassword && oc> restart`

连不上 WiFi 时自动回到门户模式，不用担心配错。

### 大模型

配三项：提供商、模型名、API Key。

| 提供商 | 值 | 一句话 |
|--------|-----|--------|
| Anthropic (Claude) | `anthropic` | 默认，全球最强 |
| 通义千问 (DashScope) | `qwen` | 国内直连，不需要代理 |
| OpenAI (GPT) | `openai` | 经典选择 |
| DeepSeek | `deepseek` | 国产性价比之王 |
| Gemini | `gemini` | Google 出品 |
| Groq | `groq` | 推理速度天花板 |
| 智谱 (GLM) | `zhipu` | 中文能力强 |
| vLLM | `vllm` | 自部署，需设 base_url |

国内用户推荐通义千问，不需要代理就能直连：
```
oc> set_model_provider qwen
oc> set_model qwen-max
oc> set_api_key sk-xxxxx
```

### 钉钉

**创建机器人：**
1. [钉钉开放平台](https://open-dev.dingtalk.com/) → 创建应用 → 机器人类型
2. 拿到 App Key 和 App Secret
3. 启用 **Stream 模式**（选 Stream，不要选 HTTP 回调）

**填凭据：**
- 门户：钉钉标签页填入 → 保存
- CLI：`oc> set_dingtalk dingxxxx secretxxx`

### 联网搜索

**创建百炼搜索应用：**
1. [百炼平台](https://bailian.console.aliyun.com/) → 创建应用 → 搜索增强
2. 拿到 App ID 和 API Key

**填配置：** `oc> set_search_key sk-xxxxx && oc> set_bailian_app_id 758d9af4...`

### HTTP 代理

国内用户访问 Anthropic/OpenAI 可能需要代理：
```
oc> set_proxy 192.168.1.83 7897    # Clash/V2Ray 的 HTTP 端口
oc> clear_proxy                     # 不需要代理了
```

---

## CLI 命令

USB 串口 115200 波特率，`oc>` 命令行：

```
# 配置
oc> wifi_set <ssid> <pass>       oc> set_dingtalk <key> <secret>
oc> set_api_key <key>            oc> set_model <model>
oc> set_model_provider <p>       oc> set_bailian_app_id <id>
oc> set_search_key <key>         oc> set_proxy <host> <port>
oc> clear_proxy                  oc> config_show
oc> config_reset                 oc> restart

# 运维
oc> wifi_status                  oc> wifi_scan
oc> memory_read                  oc> memory_write "内容"
oc> heap_info                    oc> session_list
oc> session_clear <chat_id>
```

---

## 记忆系统

OttoClaw 所有数据都是纯文本文件，AI 可以读写：

| 文件 | 说明 |
|------|------|
| `SOUL.md` | 机器人人设 — 它是谁、性格如何 |
| `USER.md` | 你的画像 — 它记住你喜欢什么 |
| `MEMORY.md` | 长期记忆 — 跨会话保留 |
| `YYYY-MM-DD.md` | 每日笔记 — 自动记录每天的事 |
| `<chat_id>.jsonl` | 会话历史 — 每个聊天独立存档 |

你跟它说"记住我喜欢火锅"，它写进 `MEMORY.md`。几天后它主动问你"要不要推荐火锅店"。这才是陪伴。

---

## 技术架构

- **纯 C / FreeRTOS** — 没有 Linux，没有 Python，一块 ESP32-S3 搞定
- **双核分工** — Core 0 网络收发，Core 1 Agent 思考+行动
- **Anthropic 风格 tool use** — ReAct 循环，AI 自主决定调用哪些工具
- **SPIFFS 本地存储** — 记忆、会话、配置、技能全在设备上，不依赖云
- **6 舵机 LEDC PWM** — 每个关节独立控制，振荡器驱动平滑运动

详见 **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** 和 **[docs/TODO.md](docs/TODO.md)**

---

## 许可证

MIT

---

## 致谢

灵感来自 [mimiclaw](https://github.com/memovai/mimiclaw)、[OpenClaw](https://github.com/openclaw/openclaw) 和 [Nanobot](https://github.com/HKUDS/nanobot)。我们在嵌入式硬件上延续了 AI Agent 架构，同时把它做成更有实体感的设备体验。