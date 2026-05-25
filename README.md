# OttoClaw Lite

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**[中文](README.md) | [English](README_EN.md)**

> 别的机器人都在跟你说话。OttoClaw 从不打扰你。
> 它悄咪咪给你发钉钉消息 — 你可以问候它、下指令、让它跳舞。
> AI 真正控制身体：6 个舵机，每个关节都能精确操控，自主编排。

由 **闪猫科技** 研发 · 开源 Lite 版

---

## 不一样在哪

市面上的陪伴机器人全是语音交互 — 说话、播放、打断你。

**OttoClaw 走了一条完全不同的路：**

- **不打扰你** — 通过钉钉、飞书等社媒安静地跟你对话。开会时它不会突然开口，你忙的时候它就安静等着。有空了看一眼消息，回一句指令，它就动了。像养了一只不吵的小猫。
- **AI 是灵魂，不是遥控器** — 不是你说"挥手"它就挥手这么简单。大模型自主决定做什么动作、做什么表情、说什么话。你说一句"跳个舞"，AI 自己编排：先走几步 → 挥手打招呼 → 太空步 → 广播体操 → 风车 → 收尾。每一段都由 AI 自主决定，不是你写的脚本。
- **精确控制每个关节** — 除了 22 个预定义动作，AI 还可以直接指定每个舵机的角度。你说"左手到45度、右脚到120度"，它就精确到达那个位置。6个关节，6个独立舵机，AI 可以任意组合编排姿态序列。
- **6 舵机全身体** — 双手、双腿、双脚，6 个独立关节。走路、转身、跳跃、坐下、弯腰、挥手、太空步、广播体操、大风车、害羞、健身操……22 个动作每个都可调次数、速度、方向、幅度。
- **0.5W 永不关机** — USB 供电，纯 C / FreeRTOS，一块 ESP32-S3 搞定全部。24/7 在线，不需要你操心。

---

## 看看它能玩什么

### 跟它聊天

在钉钉上给它发消息，它就回复。聊什么都可以 — 问天气、搜新闻、写代码、讲故事、记备忘。它还能联网搜索、读写本地文件、记录长期记忆。

```
你: "今天杭州天气怎么样？"
OttoClaw: [调用 web_search] → "杭州今天晴，28°C，适合出门走走"
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

### 精确控制姿态（Servo Sequences Lite）

这是 Lite 版新增的能力 — 你可以直接告诉 AI 每个关节到多少度：

```
你: "左手举到160度，右脚抬到30度"
OttoClaw: [调用 self.otto.pose] → 左手160° 右脚30° → "姿势到位了！"
```

```
你: "做出一个思考的姿态"
OttoClaw: [调用 self.otto.pose] → 右手45° 右脚倾斜 → 然后再复位 → "让我想想..."
```

AI 可以自由组合多个 `pose` 调用，编排出一整套姿态序列 — 低头 → 思考 → 抬头 → 回答。每个姿态之间可以控制过渡速度。

> **Lite 版说明：** 开源 Lite 版提供 Servo Sequences Lite — AI 可以指定任意舵机角度到达目标姿态，并通过多次 pose 调用编排姿态序列。完整的 Servo Sequences 正式版（融合自编程能力，实现更多 AI 意识的物理化表达）将在后续推出。

### 和它生活

OttoClaw 会记忆。它记住你说过的事、你的偏好、每天发生的事。跨重启不会忘：

```
你: "记住我喜欢吃火锅"
OttoClaw: [调用 memory_write] → "记住了！下次推荐火锅店"

你: "我今天面试通过了"
OttoClaw: [调用 memory_append_today] → "恭喜！记到今天的笔记里了"

（几天后）
OttoClaw: "上周你面试通过了，新工作适应得怎么样？"
```

---

## 动作清单

### 预定义动作（22 个动作原语）

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

### 精确姿态控制（Servo Sequences Lite）

通过 `self.otto.pose` 工具，AI 可以直接指定 6 个舵机到达任意角度：

| 参数 | 说明 | 范围 | 默认值 |
|------|------|------|--------|
| `left_leg` | 左腿舵机角度 | 0-180 | 90 |
| `right_leg` | 右腿舵机角度 | 0-180 | 90 |
| `left_foot` | 左脚舵机角度 | 0-180 | 90 |
| `right_foot` | 右脚舵机角度 | 0-180 | 90 |
| `left_hand` | 左手舵机角度 | 0-180 | 45 |
| `right_hand` | 右手舵机角度 | 0-180 | 135 |
| `time` | 过渡时间（毫秒） | 100-3000 | 700 |

90° 是中立位置。双手默认 45°/135° 是自然下垂姿态。角度越小越朝一侧，越大越朝另一侧。

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

## 快速上手（面向新手详细指南）

### 你需要准备什么

1. 一块 **ESP32-S3 开发板** — 16MB Flash + 8MB PSRAM（常见开发板基本都满足）
2. 一套 **Otto 六舵机机器人** — 双手+双腿+双脚，6个舵机
3. 一根 **USB Type-C 数据线** — 必须是数据线，不能只是充电线
4. 一个 **大模型 API Key** — 下面配置指南里教你怎么获取
5. 可选：钉钉账号（用于钉钉聊天）
6. 可选：阿里云账号（用于联网搜索）

### 第一步：安装编译工具

OttoClaw 需要用 ESP-IDF 来编译。ESP-IDF 是乐鑫官方的开发框架。

**Mac 用户：**

```bash
# 下载 ESP-IDF v5.5
git clone -b v5.5.2 --depth 1 https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf
./install.sh esp32s3

# 每次编译前需要激活环境
source ~/esp/esp-idf/export.sh
```

**Windows 用户：** 请参考 [ESP-IDF Windows 安装指南](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/windows-setup.html)，推荐使用 ESP-IDF PowerShell 快捷方式。

**Linux 用户：**

```bash
git clone -b v5.5.2 --depth 1 https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf
./install.sh esp32s3
source ~/esp/esp-idf/export.sh
```

安装完成后，在终端输入 `idf.py` 看看有没有反应。如果输出了帮助信息，说明安装成功。

### 第二步：下载 OttoClaw 代码

```bash
git clone https://github.com/1335328414/FlashCatClaw.git
cd FlashCatClaw/mimiclaw-main

# 创建配置文件（从模板复制）
cp main/ottoclaw_secrets.h.example main/ottoclaw_secrets.h
```

### 第三步：编译固件

```bash
# 先激活 ESP-IDF 环境
source ~/esp/esp-idf/export.sh

# 设置目标芯片
idf.py set-target esp32s3

# 编译
idf.py build
```

第一次编译会比较慢（需要下载依赖），大概 5-10 分钟。后续编译只需要改动的文件，会很快。

编译成功后你会看到 `Project build complete` 的提示。

### 第四步：烧录到设备

1. 用 USB 数据线把 ESP32-S3 连到电脑
2. 找到串口设备号：
   - Mac: 在终端输入 `ls /dev/cu.usb*`，看到类似 `/dev/cu.usbmodem1101` 的就是
   - Windows: 在设备管理器里看 COM 口号，类似 `COM3`
   - Linux: `ls /dev/ttyUSB*` 或 `ls /dev/ttyACM*`
3. 烧录：

```bash
# Mac/Linux
idf.py -p /dev/cu.usbmodem1101 flash

# Windows
idf.py -p COM3 flash
```

烧录大概需要 30 秒。完成后设备会自动重启。

### 第五步：首次配网

烧录完成后，因为设备里还没有 WiFi 配置，它会**自动进入配置门户模式**。

1. 用手机或电脑搜索 WiFi 热点，找到 `OttoClaw-XXXX`（XXXX 是设备 MAC 后四位）
2. 连接这个热点（没有密码，直接连）
3. 在浏览器里打开 **http://192.168.4.1**
4. 你会看到配置页面，接下来按下面的配置指南一步步填

---

## 配置指南（一步一步教你怎么配）

### 配置 WiFi

这是最基础的配置，设备必须联网才能工作。

**通过配置门户（首次配网最方便）：**

1. 打开配置门户页面 `http://192.168.4.1`
2. 在 WiFi 标签页找到 WiFi 名称和密码输入框
3. 输入你家 WiFi 的名称和密码
4. 点击保存并重启

**通过串口 CLI（改WiFi时方便）：**

```bash
# 先用 USB 连接设备，打开串口终端（波特率 115200）
oc> wifi_set 你的WiFi名 你的WiFi密码
oc> restart
```

> 小贴士：如果 WiFi 连不上（40秒超时），设备会自动回到配置门户模式，不用担心配错。重新连热点改一下就行。

### 配置大模型

OttoClaw 需要一个大模型的 API Key 才能对话和思考。这是最关键的配置。

#### 方式一：用 Anthropic Claude（默认）

如果你能访问海外服务，Claude 是最强的选择：

1. 打开 [Anthropic Console](https://console.anthropic.com/)
2. 注册账号 → 创建 API Key → 得到类似 `sk-ant-api03-xxxxx` 的密钥
3. 在配置门户的 LLM 标签页，或者串口 CLI 里填入：

```
oc> set_model_provider anthropic
oc> set_model claude-sonnet-4-5
oc> set_api_key sk-ant-api03-xxxxx
```

> 国内用户注意：Anthropic 需要代理才能访问，请同时配置 HTTP 代理（见下方）

#### 方式二：用通义千问（国内用户推荐，不需要代理）

1. 打开 [DashScope 控制台](https://dashscope.console.aliyun.com/)
2. 注册/登录阿里云账号 → 开通 DashScope 服务
3. 在 API-KEY 管理页面创建密钥 → 得到类似 `sk-xxxxx` 的密钥
4. 配置：

```
oc> set_model_provider qwen
oc> set_model qwen-max
oc> set_api_key sk-xxxxx
```

通义千问国内直连，**不需要代理**，最省心。

#### 方式三：用 DeepSeek

1. 打开 [DeepSeek 开放平台](https://platform.deepseek.com/)
2. 注册 → 创建 API Key
3. 配置：

```
oc> set_model_provider deepseek
oc> set_model deepseek-chat
oc> set_api_key sk-xxxxx
```

DeepSeek 也支持国内直连，性价比很高。

#### 其他支持的提供商

| 提供商 | 值 | 特点 |
|--------|-----|------|
| OpenAI (GPT) | `openai` | 经典，需代理 |
| Gemini | `gemini` | Google，需代理 |
| Groq | `groq` | 推理速度天花板 |
| 智谱 (GLM) | `zhipu` | 中文能力强 |
| vLLM | `vllm` | 自己部署模型 |

### 配置钉钉

钉钉是 OttoClaw 的主要聊天入口。配置好后，你在钉钉手机端就能跟机器人对话。

#### 第一步：创建钉钉机器人应用

1. 用电脑打开 [钉钉开放平台](https://open-dev.dingtalk.com/)
2. 登录你的钉钉账号
3. 点击 **应用开发** → **创建应用**
4. 填写应用名称（随便起，比如"OttoClaw机器人"）
5. 应用类型选择 **机器人**
6. 创建完成后，你会看到两个重要信息：
   - **App Key** — 类似 `dinggxs77qeiqk8xzfhj`，这是应用的唯一标识
   - **App Secret** — 一串较长的字符，是应用的密钥
7. **关键步骤**：在机器人配置页面，找到 **消息接收模式**，选择 **Stream 模式**
   - 不要选 HTTP 回调模式！Stream 模式是钉钉最新的消息订阅方式，不需要你有公网服务器
   - Stream 模式让设备直接跟钉钉服务器建立 WebSocket 连接来收发消息
8. 在钉钉手机端，找到你创建的机器人，试着给它发一条消息

#### 第二步：把凭据填进 OttoClaw

**通过配置门户：**

打开门户 → 找到钉钉标签页 → 把 App Key 和 App Secret 填进去 → 保存

**通过串口 CLI：**

```
oc> set_dingtalk dinggxs77qeiqk8xzfhj -PK7-pa-y3rIJKRXNA22bmGvvStx3PECRdRqN6aHmHIKVf1zU7IMHZL3EmSpz0-C
```

保存后重启设备，钉钉机器人就会开始接收消息了。

### 配置联网搜索

让 OttoClaw 能搜索实时信息（新闻、天气、知识点等）。

#### 第一步：创建百炼搜索应用

1. 打开 [阿里云百炼平台](https://bailian.console.aliyun.com/)
2. 登录阿里云账号（跟 DashScope 用同一个账号就行）
3. 点击 **应用管理** → **创建应用**
4. 选择 **搜索增强** 类型
5. 创建完成后你会获得：
   - **App ID** — 类似 `758d9af49a7a460ba91ba16a7d7d767b`，搜索应用的标识
   - **API Key** — 在 DashScope 账户的 API-KEY 管理页面获取，格式 `sk-xxxxx`

#### 第二步：填入搜索配置

**通过配置门户：** 搜索标签页 → 填入 API Key 和 App ID → 保存

**通过串口 CLI：**

```
oc> set_search_key sk-xxxxx
oc> set_bailian_app_id 758d9af49a7a460ba91ba16a7d7d767b
```

### 配置 HTTP 代理（国内用户访问 Claude/OpenAI 需要）

如果你用通义千问或 DeepSeek，不需要代理，跳过这节。

如果你用 Anthropic Claude 或 OpenAI，国内网络需要代理：

```
# 假设你电脑上跑着 Clash 或 V2Ray，HTTP 端口是 7897
oc> set_proxy 192.168.1.83 7897

# 192.168.1.83 是你代理服务器的 IP
# 7897 是 HTTP 端口（不同软件默认端口不同，Clash 常见是 7890 或 7897）

# 不需要代理了就清除
oc> clear_proxy
```

代理用的是 HTTP CONNECT 隧道方式，兼容 Clash、V2Ray、Shadowsocks 等常见工具。

### 如何重新进入配置门户

WiFi 配好后设备正常启动进入聊天模式。想改设置怎么办？

**方法一：按住 BOOT 键**

1. 拔掉 USB 重新插上（或者按设备上的复位键）
2. 上电的同时**按住 BOOT 键 2 秒钟**
3. 设备进入配置门户模式，重新连 `OttoClaw-XXXX` 配置

**方法二：通过串口 CLI**

```
oc> config_reset    # 清除所有运行时配置
oc> restart         # 重启后自动进入配置门户
```

---

## CLI 命令

通过 USB 串口连接设备（波特率 115200），使用 `oc>` 命令行。

### 运行时配置

```
oc> wifi_set <ssid> <pass>        设置WiFi名称和密码
oc> set_dingtalk <key> <secret>   设置钉钉App Key和Secret
oc> set_api_key <key>             设置大模型API Key
oc> set_model <model>             设置模型名称
oc> set_model_provider <provider> 设置提供商（anthropic/qwen/openai/deepseek/gemini/groq/zhipu/vllm）
oc> set_search_key <key>          设置搜索API Key
oc> set_bailian_app_id <id>       设置百炼搜索App ID
oc> set_proxy <host> <port>       设置HTTP代理
oc> clear_proxy                   清除代理设置
oc> config_show                   显示所有当前配置
oc> config_reset                  清除所有配置（下次启动进门户）
```

### 调试与运维

```
oc> wifi_status                   显示WiFi连接状态和IP地址
oc> wifi_scan                     扫描附近的WiFi
oc> memory_read                   显示长期记忆内容
oc> memory_write "内容"           写入长期记忆
oc> heap_info                     显示可用内存
oc> session_list                  列出聊天会话
oc> session_clear <chat_id>       清除指定会话
oc> restart                       重启设备
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