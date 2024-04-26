#pragma once
#include "top.h"

#define MF_KP 0x0a
#define MF_ST 0x0b
#define MF_STP 0x0c
#define MF_ST2P 0x0d
#define MF_ST3P 0x0e



namespace MF_Decoder {

const uint8_t NUM_MSG_QUEUE_OBJECTS = 4;
const uint8_t NUM_MF_RECEIVERS = 2;
const uint8_t NUM_MF_FREQUENCIES = 6;
const uint8_t MF_MAX_DIGITS = 16;
const uint8_t MF_DECODE_TABLE_SIZE = 15;
const float MF_SAMPLE_RATE = 16000.0; /* 16000 Hz simplifies the anti-aliasing low pass filter requirements. */
const uint16_t MF_FRAME_SIZE = 320;
const uint16_t MF_ADC_BUF_LEN = (2*MF_FRAME_SIZE);
const float MIN_ADC = -2048.0;
const float SILENCE_THRESHOLD = 2.0; /* Digit detect noise floor */
const uint8_t MIN_KP_GATE_BLOCK_COUNT = 3;
const uint8_t MIN_DIGIT_BLOCK_COUNT = 2;
const uint16_t MF_INTERDIGIT_TIMEOUT = 50*5; /* 5 Seconds */


enum {MFE_OK=0, MFE_TIMEOUT};
enum {MFR_IDLE=0, MFR_WAIT_KP, MFR_KP_SILENCE, MFR_WAIT_DIGIT, MFR_WAIT_DIGIT_SILENCE, MFR_TIMEOUT, MFR_DONE, MFR_WAIT_RELEASE};

typedef void (*Mf_Callback)(uint32_t descriptor, uint8_t error_code, uint8_t digit_count, char *data);

/* Data passed in message queue from interrupt */
typedef struct queueData {
	uint32_t buffer_number;
	uint32_t receiver;
}queueData;


/* Data for each goertzel tone decoder */

typedef struct goertzelData {
	float q1;
	float q2;
	float coeff_k;
	float power;
} goertzelData;


/* MF receiver state data */

typedef struct mfData {
	bool re_arm;
	char tone_digit;
	uint8_t timer;
	uint8_t state;
	uint8_t error_code;
	uint8_t tone_block_count;
	uint8_t digit_count;
	Mf_Callback callback;
	char digits[MF_MAX_DIGITS];
	uint16_t mf_dma_buffer[MF_ADC_BUF_LEN];


} mfData;

typedef struct mfDataGoertzel {
	float goertzel_block[MF_FRAME_SIZE];
	goertzelData goertzel_data[NUM_MF_FREQUENCIES];
} mfDataGoertzel;

/*
 *  Functions
 */

class MF_Decoder {

public:
void setup(); /* Called once before RTOS is running */
void init(); /* Called once after RTOS is running */
int32_t seize(Mf_Callback callback, int channel = -1, bool re_arm=false); /* Called to seize the MF receiver */
void release(int32_t descriptor); /* Called to release the MF receiver */
void handle_buffer(ADC_HandleTypeDef *hadc, uint8_t buffer_no); /* Called by the DMA engine when half full and full.*/
void receiver_worker(void *args)  __attribute__((section(".xccmram")));

protected:

void _start_dma_transfers(uint32_t receiver_descriptor);
void _stop_dma_transfers(uint32_t receiver_descriptor);
osMessageQueueId_t _message_queue;
osMutexId_t _lock;
uint32_t _rx_in_use_bits;
mfData _mf_data[NUM_MF_RECEIVERS];
};

} /* END Namespace MF_Decoder */

extern MF_Decoder::MF_Decoder MF_decoder;
