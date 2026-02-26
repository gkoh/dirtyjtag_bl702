#ifndef UART_BRIDGE_H
#define UART_BRIDGE_H

#include <stdint.h>

void uart_bridge_usb_init(uint8_t busid);
void uart_bridge_uart_init(void);
void uart_bridge_start(void);
void uart_task(void *param);

#endif
