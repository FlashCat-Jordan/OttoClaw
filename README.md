# OttoClaw Lite

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[中文](README.md) | [English](README_EN.md)**

> 别的机器人都在跟你说话。OttoClaw 从不打扰你。
> 专为 i 人准备 — 消息交互，从不开口打扰。
> AI 真正控制身体的每一个关节：6 舵机，AI 自主编排动作，即兴创作姿态。
> 全开放配置 — 你选模型、你选通道、你选搜索，阿里云百炼一键接入。

由 **闪猫科技** 研发 · 开源 Lite 版

---

## 核心亮点

### AI Servo Sequences — AI 自主控制每一个关节

这是 OttoClaw 最核心的能力。市面上其他机器人要么是预设动作，要么是语音指令映射。OttoClaw 让 **AI 自己思考动作**，自主控制 6 个舵机到达任意角度，创造任何它想象的动作姿态：

```
你: "来一个求婚的动作"
AI 思考：求婚 = 单膝跪地 + 右手举高 + 左手放下 + 微微低头
→ 调用 self.otto.pose：右腿30° 右脚0° 右手10°(高举) 左手45°(放下)
→ 机器人执行：跪下，举右手
→ AI 回复："我跪下了，你愿意嫁给我吗？"
```

```
你: "做一个祈祷的姿势"
AI 思考：祈祷 = 双手合十 + 微微低头 + 身体稳定
→ 调用 self.otto.pose：双手合拢角度 + 脚部稳定 + 低头姿态
→ 机器人执行：双手合十
→ AI 回复："让我为你祈祷一下"
```

```
你: "表现出胜利的感觉"
AI 思考：胜利 = 双手高举 + 身体挺直 + 振奋
→ 调用 self.otto.pose：双手高举角度 + 腿部站直
→ 机器人执行：双手举起
→ AI 回复："赢了！太棒了！"
```

每次都是 AI 自主思考、自主编排，不是你写的脚本。同样一句"求婚"，每次 AI 可能设计出不同的姿态组合。**这才是 AI 真正控制身体**。

> **Lite 版** 提供 AI Servo Sequences Lite — AI 可指定任意舵机角度编排姿态序列。正式版将融合自编程能力，实现更丰富的 AI 意识物理化表达。详见 [正式版规划](#正式版规划)

### 专为 i 人 — 从不打扰你

市面上的陪伴机器人全是语音交互 — 说话、播放、打断你的注意力。

**OttoClaw 专为 i 人准备：**

- **钉钉/飞书消息交互** — 不开口说话，不打扰你。你忙的时候它安静等着，有空了看一眼消息，回一句指令，它就动了。像养了一只不吵的小猫。
- **你也可以主动问候** — 通过钉钉或社媒发消息给它，让它做动作、回答问题、搜索信息。一切都在消息里，安静、私密、不打扰。
- **AI 会自动表达情绪** — 聊天时 AI 根据语境自主触发动作和表情，不需要你下指令。它开心时会摇摆，害羞时会低头捂脸，思考时会做一个思考的姿态。

> **Lite 版** 为 i 人设计，安静不打扰。正式版还将推出 **专为 e 人** 的版本 — 嘴巴特别多，能语音对话，能主动跟你聊天。

### 全开放 — 你选模型、你选通道、你选搜索

OttoClaw 不锁定任何平台。所有配置全开放：

- **8 个大模型提供商** — Anthropic Claude、通义千问、OpenAI、DeepSeek、Gemini、Groq、智谱、vLLM。你想用哪个就用哪个，随时切换。
- **钉钉/飞书/社媒接入** — 钉钉 Stream 模式直连，不需要公网服务器。未来可扩展飞书、企微等更多通道。
- **阿里云百炼一键接入** — 搜索增强、Agent 应用、MCP 服务，打通百炼平台上的丰富生态，一个 App ID + API Key 就能用。
- **HTTP 代理支持** — Clash/V2Ray/Shadowsocks 兼容，受限网络也能正常使用。
- **配置门户 + 串口 CLI** — 首次配网用 Web 页面，运行时改设置用命令行，两种方式随时切换。
- **编译时 + 运行时两层配置** — 开发者可以编译时写默认值，普通用户运行时改，不冲突。

### 0.5W 永不关机

- USB 供电，纯 C / FreeRTOS，一块 ESP32-S3 搞定全部
- 没有 Linux，没有 Node.js，没有臃肿依赖
- 24/7 在线，0.5W 功耗，不需要你操心

---

## 能玩什么

### 即兴动作（AI Servo Sequences Lite）

你只需要一句话，AI 自己思考动作编排：

```
你: "来一个求抱抱的动作" → AI 设计姿态 → 双手张开 → 身体前倾 → "抱抱我吧！"
你: "表现出愤怒" → AI 设计姿态 → 脚部用力 + 身体前倾 → "我很生气！"
你: "做一个鞠躬" → AI 设计姿态 → 上身前倾 + 双手放低 → "向您致敬"
```

### 预定义动作（22 个动作原语）

| 行走 | walk / walk_backward / turn |
| 跳跃 | jump / updown |
| 摇摆 | swing / moonwalk |
| 姿态 | sit / bend / shake_leg / home |
| 双手 | hands_up / hands_down / hand_wave |
| 花式 | windmill / takeoff / fitness |
| 情绪 | greeting / shy |
| 套路 | radio_calisthenics / magic_circle / showcase |

### 聊天 & 搜索 & 记忆

在钉钉上跟它聊任何事 — 问天气、搜新闻、写代码、记备忘：

```
你: "今天杭州天气怎么样？"
OttoClaw: [web_search] → "杭州今天晴，28°C，适合出门走走"

你: "记住我喜欢吃火锅"
OttoClaw: [memory_write] → "记住了！"

（几天后）
OttoClaw: "要不要推荐火锅店？"  ← 它主动记得你说过的事
```

---

## 交互方式

- **钉钉** — 主要聊天入口。Stream 模式直连，不需要公网服务器。安静不打扰，专为 i 人。
- **WebSocket** — 端口 18789。浏览器聊天页 + 设置页 + WebSocket API，开发者可以接自己的前端或桥接服务。
- **串口 CLI** — `oc>` 命令行，本地运维和配置。

---

## 正式版规划

开源 Lite 版展示核心能力。正式版将推出更多：

- **AI Servo Sequences 正式版** — 融合自编程能力，AI 不只是编排姿态序列，还能自编程创造更复杂的连续动作编排。AI 意识的物理化表达更丰富。
- **专为 e 人** — 嘴巴特别多的版本。能语音对话，能主动跟你聊天，适合喜欢热闹的人。i 人版本安静不打扰，e 人版本热情会说话。
- **更多通道** — 飞书、企微、更多社媒接入。
- **更丰富的百炼生态接入** — Agent 应用、MCP 服务一键配置。

---

## 快速上手（面向新手详细指南）

### 你需要准备什么

1. 一块 **ESP32-S3 开发板**（16MB Flash + 8MB PSRAM）+ Otto 六舵机机器人
2. 一根 **USB Type-C 数据线**（必须是数据线）
3. 一个 **大模型 API Key**（下面教你怎么获取）
4. 可选：钉钉账号、阿里云账号

### 第一步：安装编译工具

```bash
# Mac/Linux
git clone -b v5.5.2 --depth 1 https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && ./install.sh esp32s3
source ~/esp/esp-idf/export.sh   # 每次编译前激活
```

**Windows:** 参考 [ESP-IDF Windows 安装指南](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/windows-setup.html)

终端输入 `idf.py` 有帮助信息输出 = 安装成功。

### 第二步：下载代码

```bash
git clone https://github.com/1335328414/FlashCatClaw.git
cd FlashCatClaw/mimiclaw-main
cp main/ottoclaw_secrets.h.example main/ottoclaw_secrets.h
```

### 第三步：编译烧录

```bash
idf.py set-target esp32s3 && idf.py build
idf.py -p PORT flash   # PORT = /dev/cu.usbmodem1101 (Mac) 或 COM3 (Windows)
```

### 第四步：首次配网

烧录后没有 WiFi → 自动进入配置门户：
1. 手机连 `OttoClaw-XXXX` WiFi（无密码）
2. 打开 http://192.168.4.1
3. 在网页上配置一切 → 保存并重启

随时重新进门户：**按住 BOOT 键 2秒**

---

## 配置指南

两层配置：编译时默认值（`ottoclaw_secrets.h`）+ 运行时覆盖（NVS/门户/CLI）。运行时优先。

### WiFi

**门户：** 填 WiFi 名称密码 → 保存重启

**CLI：** `oc> wifi_set MySSID MyPassword && oc> restart`

连不上自动回到门户，不用担心配错。

### 大模型

配三项：提供商、模型名、API Key。

**通义千问（国内推荐，不需要代理）：**
1. [DashScope 控制台](https://dashscope.console.aliyun.com/) → 开通服务 → 创建 API Key
2. `oc> set_model_provider qwen && oc> set_model qwen-max && oc> set_api_key sk-xxxxx`

**Anthropic Claude（需代理）：**
1. [Anthropic Console](https://console.anthropic.com/) → 创建 API Key
2. `oc> set_model_provider anthropic && oc> set_model claude-sonnet-4-5 && oc> set_api_key sk-ant-api03-xxxxx`

**DeepSeek（国内直连，性价比高）：**
1. [DeepSeek 开放平台](https://platform.deepseek.com/) → 创建 API Key
2. `oc> set_model_provider deepseek && oc> set_model deepseek-chat && oc> set_api_key sk-xxxxx`

| 其他 | 值 | 特点 |
|------|-----|------|
| OpenAI | `openai` | 经典 |
| Gemini | `gemini` | Google |
| Groq | `groq` | 推理极快 |
| 智谱 | `zhipu` | 中文强 |
| vLLM | `vllm` | 自部署 |

### 钉钉

1. [钉钉开放平台](https://open-dev.dingtalk.com/) → 创建应用 → 机器人类型
2. 拿到 App Key + App Secret
3. **关键**：消息接收模式选 **Stream**（不要选 HTTP 回调）
4. `oc> set_dingtalk dingxxxx secretxxx`

### 阿里云百炼（搜索 + Agent 应用 + MCP）

1. [百炼平台](https://bailian.console.aliyun.com/) → 创建应用 → 搜索增强类型
2. 拿到 App ID + API Key
3. `oc> set_search_key sk-xxxxx && oc> set_bailian_app_id 758d9af4...`

一键接入百炼平台生态，搜索增强、Agent 应用、MCP 服务都可用。

### HTTP 代理

国内用户访问 Claude/OpenAI 需要代理：
```
oc> set_proxy 192.168.1.83 7897    # Clash/V2Ray HTTP 端口
oc> clear_proxy                     # 不需要了
```

### 重新进配置门户

- **按住 BOOT 键 2秒** 上电 → 进门户
- CLI: `oc> config_reset && oc> restart`

---

## CLI 命令

USB 串口 115200 波特率，`oc>` 命令行：

```
oc> wifi_set <ssid> <pass>        oc> set_dingtalk <key> <secret>
oc> set_api_key <key>             oc> set_model <model>
oc> set_model_provider <p>        oc> set_bailian_app_id <id>
oc> set_search_key <key>          oc> set_proxy <host> <port>
oc> clear_proxy                   oc> config_show
oc> config_reset                  oc> restart
oc> wifi_status                   oc> wifi_scan
oc> memory_read                   oc> memory_write "内容"
oc> heap_info                     oc> session_list
```

---

## 记忆系统

所有数据纯文本存储，AI 可读写：

| 文件 | 说明 |
|------|------|
| `SOUL.md` | 机器人人设 |
| `USER.md` | 你的偏好画像 |
| `MEMORY.md` | 长期记忆（跨会话） |
| `YYYY-MM-DD.md` | 每日笔记（自动） |
| `<chat_id>.jsonl` | 会话历史 |

---

## 技术架构

- **纯 C / FreeRTOS** — 一块 ESP32-S3 搞定
- **双核** — Core 0 网络 I/O，Core 1 Agent 思考+行动
- **Anthropic tool use ReAct 循环** — AI 自主决定调用哪些工具
- **6 舵机 LEDC PWM** — 每个关节独立控制
- **SPIFFS 本地存储** — 不依赖云

详见 **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** 和 **[docs/TODO.md](docs/TODO.md)**

---

## 许可证

MIT

---

## 致谢

灵感来自 [mimiclaw](https://github.com/memovai/mimiclaw)、[OpenClaw](https://github.com/openclaw/openclaw) 和 [Nanobot](https://github.com/HKUDS/nanobot)。