# MiaomiaoClaw: $5 芯片上的口袋 AI 助理

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![DeepWiki](https://img.shields.io/badge/DeepWiki-mimiclaw-blue.svg)](https://deepwiki.com/memovai/mimiclaw)
[![Discord](https://img.shields.io/badge/Discord-mimiclaw-5865F2?logo=discord&logoColor=white)](https://discord.gg/r8ZxSvB8Yr)
[![X](https://img.shields.io/badge/X-@ssslvky-black?logo=x)](https://x.com/ssslvky)

**[English](README.md) | [中文](README_CN.md)**

<p align="center">
  <img src="assets/banner.png" alt="MiaomiaoClaw" width="480" />
</p>

**纯 C 的 ESP32-S3 AI 助理，内置配网门户、钉钉聊天、WebSocket 接入、LCD 情绪与 Otto 动作反馈。**

MiaomiaoClaw 把一块小小的 ESP32-S3 开发板变成你的私人 AI 助理。插上 USB 供电，连上 WiFi，通过设备自带配置门户完成初始化，然后就可以通过钉钉或 WebSocket 与它交互。Claude 在设备上的 Agent 循环中运行，能够调用工具、读取本地记忆，并同步驱动 LCD 表情和 Otto 机器人动作。

## 认识 MiaomiaoClaw

- **小巧** — 没有 Linux，没有 Node.js，没有臃肿依赖，纯 C
- **好接入** — 自带配置门户、钉钉机器人、WebSocket 网关
- **更有生命感** — LCD 情绪状态 + Otto 动作反馈
- **忠诚** — 从记忆中学习，跨重启也不会忘
- **能干** — USB 供电，0.5W，24/7 运行

## 工作原理

![](assets/mimiclaw.png)

外部消息先由钉钉或 WebSocket 通道接收，再统一写入内部 message bus。ESP32-S3 读取本地配置、记忆和会话历史，构造上下文后调用大模型，必要时执行工具调用，最后把结果回发到原通道，并在聊天过程中更新 LCD 情绪、触发 Otto 的简短动作。

## 快速开始

### 你需要

- 一块 **ESP32-S3 开发板**，16MB Flash + 8MB PSRAM
- 一根 **USB Type-C 数据线**
- 一个 **LLM API Key**（默认 Anthropic，也支持配置其他 provider）
- 可选：**钉钉应用凭据**
- 可选：**Brave Search API Key**

### 安装

```bash
# 需要先安装 ESP-IDF v5.5+:
# https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/

git clone https://github.com/memovai/mimiclaw.git
cd mimiclaw

idf.py set-target esp32s3
```

### 配置

MiaomiaoClaw 使用**两层配置**：`mimi_secrets.h` 提供编译时默认值，运行时配置保存在 NVS 中。你既可以用串口 CLI，也可以用设备自带的 Web 配置页修改设置。

```bash
cp main/mimi_secrets.h.example main/mimi_secrets.h
```

如果你想写入编译时默认值，编辑 `main/mimi_secrets.h`：

```c
#define MIMI_SECRET_WIFI_SSID              "你的WiFi名"
#define MIMI_SECRET_WIFI_PASS              "你的WiFi密码"
#define MIMI_SECRET_DINGTALK_APP_KEY       "dingxxxx"
#define MIMI_SECRET_DINGTALK_APP_SECRET    "xxxx"
#define MIMI_SECRET_API_KEY                "sk-ant-api03-xxxxx"
#define MIMI_SECRET_MODEL_PROVIDER         "anthropic"
#define MIMI_SECRET_MODEL                  "claude-opus-4-5"
#define MIMI_SECRET_API_BASE_URL           ""
#define MIMI_SECRET_SEARCH_KEY             ""
#define MIMI_SECRET_PROXY_HOST             ""
#define MIMI_SECRET_PROXY_PORT             ""
```

然后编译烧录：

```bash
idf.py fullclean && idf.py build
idf.py -p PORT flash monitor
```

### 首次启动

如果设备里还没有 WiFi 配置，或者上电时按住 BOOT 键，设备会进入 **配置门户模式**：

1. 用手机或电脑连接热点 `MiaomiaoClaw-XXXX`
2. 打开 `http://192.168.4.1`
3. 配置 WiFi、LLM、钉钉、代理/搜索，并可直接测试 Otto 动作
4. 保存并重启

### CLI 命令

通过串口连接即可配置和调试。

**运行时配置**（存入 NVS，覆盖编译时默认值）：

```
mimi> wifi_set MySSID MyPassword
mimi> set_dingtalk dingxxx secretxxx
mimi> set_api_key sk-ant-api03-...
mimi> set_model claude-sonnet-4-5
mimi> set_model_provider anthropic
mimi> set_proxy 192.168.1.83 7897
mimi> clear_proxy
mimi> set_search_key BSA...
mimi> config_show
mimi> config_reset
```

**调试与运维：**

```
mimi> wifi_status
mimi> wifi_scan
mimi> memory_read
mimi> memory_write "内容"
mimi> heap_info
mimi> session_list
mimi> session_clear 12345
mimi> restart
```

## 接入方式

- **钉钉** — 当前这个分支的主要移动端聊天入口
- **WebSocket 网关** — 端口 `18789`，适合局域网页面或桥接服务
- **串口 CLI** — 本地维护与调试

## 记忆

MiaomiaoClaw 把主要数据都存成可直接读取的文本文件：

| 文件 | 说明 |
|------|------|
| `SOUL.md` | 机器人的人设 |
| `USER.md` | 用户画像与偏好 |
| `IDENTITY.md` | 身份/产品说明 |
| `AGENTS.md` | Agent 行为指导 |
| `TOOLS.md` | 工具使用说明 |
| `MEMORY.md` | 长期记忆 |
| `YYYY-MM-DD.md` | 每日笔记 |
| `<chat_id>.jsonl` | 会话历史 |

## 工具

MiaomiaoClaw 使用 Anthropic 风格的 tool use / ReAct 循环。

| 工具 | 说明 |
|------|------|
| `web_search` | 通过 Brave Search API 搜索实时信息 |
| `get_current_time` | 获取当前时间并设置系统时钟 |
| `read_file` / `write_file` / `edit_file` / `list_dir` | SPIFFS 文件访问 |
| `memory_write` / `memory_append_today` | 持久化记忆写入 |
| `self.otto.action` | 触发机器人动作 |

## 其他内置能力

- **配置门户** — AP 模式 + Web UI，适合首次配网和运行时配置
- **LCD 情绪系统** — 聊天时的思考/说话/基础情绪展示
- **Otto 动作反馈** — 情绪动作和测试面板
- **双核架构** — 网络 I/O 与 Agent 处理分布在不同核心
- **HTTP 代理** — CONNECT 隧道支持受限网络
- **Cron / Heartbeat 服务** — 固件内已包含的后台服务
- **语音转写模块** — 固件已包含，但尚未完全接入主聊天链路

## 开发者

技术细节在 `docs/` 文件夹：

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — 当前系统架构和模块图
- **[docs/TODO.md](docs/TODO.md)** — 当前这个分支的优化路线图

## 许可证

MIT

## 致谢

灵感来自 [OpenClaw](https://github.com/openclaw/openclaw) 和 [Nanobot](https://github.com/HKUDS/nanobot)。MiaomiaoClaw 在嵌入式硬件上延续了 AI Agent 架构，同时把它进一步做成更有实体感的设备体验。

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=memovai/mimiclaw&type=Date)](https://star-history.com/#memovai/mimiclaw&Date)
