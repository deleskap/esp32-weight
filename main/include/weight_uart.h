#ifndef WEIGHT_UART_H
#define WEIGHT_UART_H

#include "common.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t weight_uart_init(void);
bool weight_uart_read(float *weight);

#ifdef __cplusplus
}
#endif

#endif // WEIGHT_UART_H
