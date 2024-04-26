#pragma once
#include "top.h"
#include "ring_buffer.h"


namespace Uart {

const uint8_t RX_BUFFER_SIZE = 32;



class Uart {
public:
	void setup(UART_HandleTypeDef *uh) {this->uart = uh;};
	void rx_int(char c) {this->_rx_rb.put(c); return; };
	void putc(char c);
	void flush(void) {return;}
	char getc(void) { return (char) this->_rx_rb.get(); };
	char peek(void) { return (char) this->_rx_rb.peek(); };
	char available(void) { return this->_rx_rb.available(); };
	void rx_flush() {this->_rx_rb.reset(); return; };
protected:
	UART_HandleTypeDef *uart;
	RingBuffer::RingBuffer<volatile char, RX_BUFFER_SIZE> _rx_rb;

};

} // End namespace Uart

extern Uart::Uart Console_uart;
extern void __io_putchar(char c);
