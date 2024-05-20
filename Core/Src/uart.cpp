#include "top.h"
#include "uart.h"


Uart::Uart Console_uart;

/* Make printf work with designated console UART */
extern "C" {
int __io_putchar(int c)
{
	const uint8_t rtn = '\r';
	uint8_t ch = (uint8_t) c;
	/* Prepend an \r if there is an \n */
	if(ch == '\n') {
		HAL_UART_Transmit(&huart5, &rtn, 1, 10);
	}
	HAL_UART_Transmit(&huart5, (uint8_t *) &ch, 1, 10);
	return c;
}
}

namespace Uart {

void Uart::putc(char c) {
	if(this->uart) {
		__io_putchar((int) c);
	}
}


void Uart::error_handler(UART_HandleTypeDef *uh) {

}

} // End namespace Uart_Rx
