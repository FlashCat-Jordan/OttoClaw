#pragma once

#include "esp_err.h"

/**
 * Initialize and start the serial CLI (esp_console based).
 * Registers all commands and starts the REPL on USB-JTAG serial.
 */
esp_err_t serial_cli_init(void);
