# OttoClaw Lite

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[中文](README.md) | [English](README_EN.md)**

> 当所有机器人都在试图跟你说话时，OttoClaw 选择安静。
> 专为 i 人设计 — 消息交互，绝不打扰。
> AI 自主控制身体的每一个关节：6 舵机，即兴编排，自由创作姿态。
> 全开放架构 — 自选模型、自选通道、自选搜索，阿里云百炼一键接入。

由 **闪猫科技** 研发 · 开源 Lite 版

---

## 核心亮点

### AI Servo Sequences — AI 自主控制每一个关节

市面上的机器人或依赖预设动作，或靠语音指令映射。OttoClaw 赋予 AI **自主思考动作**的能力：大模型根据语义理解，自主决定 6 个舵机到达何种角度，创造任何它所想象的动作姿态。

```
用户: "来一个求婚的动作"
AI 推理: 求婚 = 单膝跪地 + 右手高举 + 左手放下 + 微微低头
→ 调用 self.otto.pose: 右腿30° 右脚0° 右手10°(高举) 左手45°(放下)
→ 机器人执行: 单膝跪地，右手高举
→ AI 回复: "我跪下了，你愿意嫁给我吗？"
```

```
用户: "做一个祈祷的姿势"
AI 推理: 祈祷 = 双手合十 + 微微低头 + 身体稳定
→ 调用 self.otto.pose: 双手合拢角度 + 脚部稳定 + 低头姿态
→ 机器人执行: 双手合十
→ AI 回复: "让我为你祈祷一下"
```

```
用户: "表现出胜利的感觉"
AI 推理: 胜利 = 双手高举 + 身体挺直 + 振奋
→ 调用 self.otto.pose: 双手高举角度 + 腿部站直
→ 机器人执行: 双手举起，挺直站立
→ AI 回复: "赢了！太棒了！"
```

以上所有姿态均由 AI 自主推理、自主编排，而非预设脚本。同样的"求婚"请求，AI 每次可能设计出不同的姿态组合。**这才是 AI 真正控制身体**。

> Lite 版提供 AI Servo Sequences Lite — AI 可指定任意舵机角度并编排姿态序列。正式版将融合自编程能力，实现更丰富的 AI 意识物理化表达。详见 [正式版规划](#正式版规划)

### 专为 i 人 — 从不打扰

当前市面上的陪伴机器人普遍采用语音交互模式 — 说话、播放、打断注意力。

**OttoClaw 专为 i 人设计，采用完全不同的交互哲学：**

- **钉钉/飞书消息交互** — 不开口、不打扰。忙碌时它安静等候，空闲时看一眼消息、回一句指令即可触发响应。安静如猫，始终在场。
- **双向主动交互** — 用户可通过钉钉或社媒向 OttoClaw 发送消息，触发动作、获取回答、搜索信息。所有交互均停留在消息层面，安静、私密、无干扰。
- **AI 自主情绪表达** — 聊天过程中，AI 根据语境自主触发动作与表情，无需用户下达指令。开心时摇摆，害羞时低头掩面，思考时做出沉思姿态 — 情绪表达是自发的，而非被动的。

> Lite 版为 i 人设计，安静不打扰。正式版将推出 **e 人版本** — 具备语音对话能力，可主动发起聊天，适合偏好热闹互动的用户。

### 全开放架构 — 自选模型、自选通道、自选搜索

OttoClaw 不锁定任何平台，所有配置完全开放：

- **8 个大模型提供商** — Anthropic Claude、通义千问、OpenAI、DeepSeek、Gemini、Groq、智谱、vLLM。自由选择，随时切换。
- **钉钉/飞书/社媒接入** — 钉钉 Stream 模式直连，无需公网服务器。飞书、企微等更多通道持续扩展中。
- **阿里云百炼一键接入** — 搜索增强、Agent 应用、MCP 服务，一个 App ID + API Key 即可打通百炼平台丰富生态。
- **HTTP 代理支持** — Clash/V2Ray/Shadowsocks 兼容，受限网络环境下亦可正常使用。
- **配置门户 + 串口 CLI 双入口** — 首次配网使用 Web 页面，运行时调整使用命令行，两种方式自由切换。
- **编译时 + 运行时两层配置** — 开发者可在编译时写入默认值，普通用户可在运行时覆盖，互不干扰。

### 0.5W 永不关机

- USB 供电，纯 C / FreeRTOS，单块 ESP32-S3 即可运行全部功能
- 无 Linux、无 Node.js、无臃肿依赖
- 24/7 在线，0.5W 功耗，无需维护

### 硬件全栈开源

本固件可运行于 **闪猫一号 OttoRobot AI 版官方开发板**。硬件部分同样开源：

- **PCB + BOM** — 立创开源硬件平台一键下单：[oshwhub.com/txp666/ottorobot](https://oshwhub.com/txp666/ottorobot)
- **3D 打印外壳** — STL 文件免费下载：[MakerWorld @shanmaotech](https://makerworld.com.cn/@shanmaotech)
- **完整组装教程** — 从元器件采购到焊接、组装、烧录，30 分钟即可完成：[ottodiy.tech](https://ottodiy.tech)

全套硬件开源文档（9 个章节：产品介绍、能力对比、采购指南、焊接教程、组装教程、烧录指南、使用说明、二次创作、常见问题）详见 [闪猫侠机器人开源生态](https://shanmaotech.cn/ottodiy/)。

---

## 功能概览

### 即兴动作创作（AI Servo Sequences Lite）

用户仅需一句话，AI 即自主完成从语义理解到姿态设计的全过程：

```
"来一个求抱抱的动作" → AI 推理姿态 → 双手张开 + 身体前倾 → "抱抱我吧！"
"表现出愤怒" → AI 推理姿态 → 脚部用力 + 身体前倾 → "我很生气！"
"做一个鞠躬" → AI 推理姿态 → 上身前倾 + 双手放低 → "向您致敬"
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

### 对话、搜索与记忆

通过钉钉与 OttoClaw 交互，支持对话、联网搜索、长期记忆：

```
用户: "今天杭州天气怎么样？"
OttoClaw: [调用 web_search] → "杭州今天晴，28°C，适合出门走走"

用户: "记住我喜欢吃火锅"
OttoClaw: [调用 memory_write] → "已记录"

（数日后）
OttoClaw: "要不要推荐一家火锅店？" ← 跨重启仍记得用户偏好
```

---

## 交互方式

- **钉钉** — 主聊天入口。Stream 模式直连，无需公网服务器。安静不打扰，专为 i 人。
- **WebSocket** — 端口 18789。内置聊天页、设置页及 WebSocket API，可供开发者接入自有前端或桥接服务。
- **串口 CLI** — `oc>` 命令行，本地运维与配置。

---

## 正式版规划

开源 Lite 版展示核心能力。正式版将推出以下增强：

- **AI Servo Sequences 正式版** — 融合自编程能力，AI 不仅能编排姿态序列，还可自编程创造更复杂的连续动作编排。AI 意识的物理化表达将更加丰富。
- **e 人版本** — 具备语音对话能力，可主动发起聊天。i 人版本安静不打扰，e 人版本热情且善谈。
- **更多通道** — 飞书、企微及更多社媒接入。
- **百炼生态深度接入** — Agent 应用、MCP 服务一键配置，生态持续扩展。

---

## 快速上手（新手指南）

### 前置要求

1. ESP32-S3 开发板（16MB Flash + 8MB PSRAM）+ Otto 六舵机机器人套件
2. USB Type-C 数据线（须支持数据传输）
3. 大模型 API Key（获取方式见下方配置指南）
4. 可选：钉钉账号、阿里云账号

### 第一步：安装编译工具

```bash
# Mac / Linux
git clone -b v5.5.2 --depth 1 https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && ./install.sh esp32s3
source ~/esp/esp-idf/export.sh   # 每次编译前需执行
```

Windows 用户请参考 [ESP-IDF Windows 安装指南](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/windows-setup.html)

安装完成后在终端执行 `idf.py`，若输出帮助信息则表示安装成功。

### 第二步：获取代码

```bash
git clone https://github.com/1335328414/FlashCatClaw.git
cd FlashCatClaw/mimiclaw-main
cp main/ottoclaw_secrets.h.example main/ottoclaw_secrets.h
```

### 第三步：编译与烧录

```bash
idf.py set-target esp32s3 && idf.py build
idf.py -p PORT flash
```

其中 PORT 为串口设备路径：Mac 一般为 `/dev/cu.usbmodem1101`，Windows 一般为 `COM3`。

### 第四步：首次配网

烧录完成后，设备因无 WiFi 配置将自动进入配置门户模式：

1. 手机连接热点 `OttoClaw-XXXX`（无密码）
2. 浏览器打开 http://192.168.4.1
3. 在配置页面填写各项设置 → 保存并重启

需要重新进入配置门户时，**上电时按住 BOOT 键 2 秒**即可。

---

## 配置指南

OttoClaw 采用两层配置机制：编译时默认值（`ottoclaw_secrets.h`）与运行时覆盖（NVS / 配置门户 / CLI）。运行时设置优先于编译时默认值。

### WiFi

**配置门户：** 填入 WiFi 名称及密码 → 保存并重启。

**CLI：** `oc> wifi_set MySSID MyPassword && oc> restart`

WiFi 连接失败时设备自动回退至配置门户模式，无需担心误配置。

### 大模型

需配置三项：提供商、模型名称、API Key。

**通义千问（国内用户推荐，无需代理）：**
1. 登录 [DashScope 控制台](https://dashscope.console.aliyun.com/) → 开通服务 → 创建 API Key
2. 执行：`oc> set_model_provider qwen && oc> set_model qwen-max && oc> set_api_key sk-xxxxx`

**Anthropic Claude（需配置代理）：**
1. 登录 [Anthropic Console](https://console.anthropic.com/) → 创建 API Key
2. 执行：`oc> set_model_provider anthropic && oc> set_model claude-sonnet-4-5 && oc> set_api_key sk-ant-api03-xxxxx`

**DeepSeek（国内直连，性价比高）：**
1. 登录 [DeepSeek 开放平台](https://platform.deepseek.com/) → 创建 API Key
2. 执行：`oc> set_model_provider deepseek && oc> set_model deepseek-chat && oc> set_api_key sk-xxxxx`

| 其他提供商 | 值 | 特点 |
|-----------|-----|------|
| OpenAI | `openai` | 经典，需代理 |
| Gemini | `gemini` | Google，需代理 |
| Groq | `groq` | 推理速度极快 |
| 智谱 (GLM) | `zhipu` | 中文能力强 |
| vLLM | `vllm` | 自部署模型服务 |

### 钉钉

1. 登录 [钉钉开放平台](https://open-dev.dingtalk.com/) → 创建应用 → 选择机器人类型
2. 获取 App Key 与 App Secret
3. **重要**：消息接收模式须选择 **Stream 模式**（切勿选择 HTTP 回调）
4. 执行：`oc> set_dingtalk dingxxxx secretxxx`

Stream 模式使设备直接与钉钉服务器建立 WebSocket 连接，无需公网服务器或回调地址。

### 阿里云百炼（搜索 + Agent 应用 + MCP）

1. 登录 [百炼平台](https://bailian.console.aliyun.com/) → 创建应用 → 选择搜索增强类型
2. 获取 App ID 与 API Key
3. 执行：`oc> set_search_key sk-xxxxx && oc> set_bailian_app_id 758d9af4...`

一个 App ID + API Key 即可接入百炼平台生态，搜索增强、Agent 应用、MCP 服务均可使用。

### HTTP 代理

国内用户访问 Claude / OpenAI 可能需要配置代理：
```
oc> set_proxy 192.168.1.83 7897    # Clash/V2Ray HTTP 端口
oc> clear_proxy                     # 移除代理设置
```

### 重新进入配置门户

- 上电时**按住 BOOT 键 2 秒** → 进入门户模式
- CLI 方式：`oc> config_reset && oc> restart`

---

## CLI 命令

USB 串口连接（波特率 115200），`oc>` 命令行：

```
oc> wifi_set <ssid> <pass>        设置 WiFi 名称与密码
oc> set_dingtalk <key> <secret>   设置钉钉凭据
oc> set_api_key <key>             设置大模型 API Key
oc> set_model <model>             设置模型名称
oc> set_model_provider <provider> 设置提供商
oc> set_search_key <key>          设置搜索 API Key
oc> set_bailian_app_id <id>       设置百炼 App ID
oc> set_proxy <host> <port>       设置 HTTP 代理
oc> clear_proxy                   移除代理设置
oc> config_show                   显示当前配置
oc> config_reset                  清除运行时配置
oc> restart                       重启设备
oc> wifi_status                   显示 WiFi 状态与 IP
oc> wifi_scan                     扫描附近 WiFi
oc> memory_read                   显示长期记忆内容
oc> memory_write "内容"           写入长期记忆
oc> heap_info                     显示可用堆内存
oc> session_list                  列出聊天会话
```

---

## 记忆系统

所有数据以纯文本文件存储于 SPIFFS，AI 可读写：

| 文件 | 说明 |
|------|------|
| `SOUL.md` | 机器人人设与性格 |
| `USER.md` | 用户偏好画像 |
| `MEMORY.md` | 长期记忆（跨会话保留） |
| `YYYY-MM-DD.md` | 每日笔记（自动生成） |
| `<chat_id>.jsonl` | 会话历史（按聊天独立存档） |

---

## 技术架构

- **纯 C / FreeRTOS** — 单块 ESP32-S3 运行全部功能
- **双核架构** — Core 0 处理网络 I/O，Core 1 运行 Agent 循环
- **Anthropic tool use / ReAct 循环** — AI 自主决定工具调用与编排
- **6 舵机 LEDC PWM** — 每个关节独立控制，振荡器驱动平滑运动
- **SPIFFS 本地存储** — 记忆、会话、配置、技能均在设备本地，不依赖云端

详见 **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** 与 **[docs/TODO.md](docs/TODO.md)**

---

## 许可证

MIT

---

## 致谢

灵感源自 [mimiclaw](https://github.com/memovai/mimiclaw)、[OpenClaw](https://github.com/openclaw/openclaw) 与 [Nanobot](https://github.com/HKUDS/nanobot)。我们将 AI Agent 架构带入嵌入式硬件，并赋予其更具实体感的设备体验。