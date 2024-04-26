#pragma once
#include <stdint.h>

namespace RingBuffer {


template <typename T, int size> class RingBuffer {
public:
	/* When placed in the .cpp file, the linker can't find a reference to put() so it is in-lined here */
	inline void put(char c) {if(this->_check_full()) {return;} else {uint32_t next = this->_next_position(this->_head); this->_buffer[this->_head] = c; this->_head = next;}};
	bool available(void);
	T get(void);
	T peek(void);
	void reset(void);


protected:
	inline uint32_t _next_position(uint32_t cur_pos) {return (((cur_pos + 1) >= size) ? 0 : cur_pos + 1 );};
	inline bool _check_full(void) {return(_next_position(_head) == _tail);};
	inline bool _check_empty(void){return(_tail == _head);};
	volatile uint32_t _head;
	volatile uint32_t _tail;
	T _buffer[size];

};


/*
 * Get a character from the ring buffer
 */

template <typename T, int size>
T RingBuffer<T, size>::get() {
	if(this->_check_empty()) {
		return 0;
	}
	uint32_t next = this->_next_position(this->_tail);
	char res = this->_buffer[this->_tail];
	this->_tail = next;
	return res;
}

/*
 * See if there is something in the ring buffer
 */

template <typename T, int size>
bool RingBuffer<T, size>::available(void) {
	return !(this->_check_empty());
}

/*
 * Return the next character in the ring buffer.
 * The buffer must not be empty for this to work properly.
 */

template <typename T, int size>
T RingBuffer<T, size>::peek(void) {
	return this->_buffer[this->_tail];
}

/*
 * Reset the ring buffer
 */

template <typename T, int size>
void RingBuffer<T, size>::reset(void) {
	this->_head = this->_tail = 0;

}


} /* End namespace UART_RB */



