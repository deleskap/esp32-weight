#include "weight_uart.h"
#include <stdlib.h>

#define WEIGHT_UART_PORT UART_NUM_1
#define WEIGHT_UART_TX_PIN GPIO_NUM_17
#define WEIGHT_UART_RX_PIN GPIO_NUM_16
#define WEIGHT_UART_BAUDRATE 9600
#define WEIGHT_BUF_SIZE 128

esp_err_t weight_uart_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = WEIGHT_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(WEIGHT_UART_PORT, WEIGHT_BUF_SIZE, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = uart_param_config(WEIGHT_UART_PORT, &uart_config);
    if (ret != ESP_OK) {
        return ret;
    }

    return uart_set_pin(WEIGHT_UART_PORT, WEIGHT_UART_TX_PIN, WEIGHT_UART_RX_PIN,
                        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

bool weight_uart_read(float *weight) {
    uint8_t data[WEIGHT_BUF_SIZE];
    int len = uart_read_bytes(WEIGHT_UART_PORT, data, WEIGHT_BUF_SIZE - 1, pdMS_TO_TICKS(100));
    if (len > 0) {
        data[len] = '\0';
        char *end = NULL;
        float value = strtof((const char *)data, &end);
        if (end != (char *)data) {
            *weight = value;
            return true;
        }
    }
    return false;
}
