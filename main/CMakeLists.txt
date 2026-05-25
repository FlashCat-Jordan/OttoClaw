idf_component_register(
    SRCS
        "ottoclaw.c"
        "bus/message_bus.c"
        "wifi/wifi_manager.c"
        "dingtalk/dingtalk_bot.c"
        "llm/llm_proxy.c"
        "agent/agent_loop.c"
        "agent/context_builder.c"
        "memory/memory_store.c"
        "memory/session_mgr.c"
        "gateway/ws_server.c"
        "cli/serial_cli.c"
        "proxy/http_proxy.c"
        "tools/tool_registry.c"
        "tools/tool_web_search.c"
        "tools/tool_get_time.c"
        "tools/tool_files.c"
        "tools/tool_otto.c"
        "otto/oscillator.c"
        "otto/otto_movements.c"
        "lcd/lcd_display.c"
        "lcd/font_chinese_14.c"
        "skills/skills.c"
        "cron/cron_service.c"
        "heartbeat/heartbeat_service.c"
        "voice/voice_transcription.c"
        "qrcode/qrcode.c"
        "config_portal/config_portal.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash esp_wifi esp_netif esp_http_client esp_http_server
        esp_event json spiffs console vfs esp-tls
        esp_lcd driver spi_flash esp_timer tcp_transport lwip
    PRIV_REQUIRES
        lvgl
)
