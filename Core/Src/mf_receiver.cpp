
#include <math.h>
#include <mf_receiver.h>
#include "logging.h"
#include "util.h"


/* References:
*
* https://github.com/AI5GW/Goertzel
*
* Notes:
*
* Tested with a Sage 930A using tone lengths: KP:100ms, Other digits: 70ms
*/

MF_Decoder::MF_Decoder MF_decoder;



namespace MF_Decoder {




const float PI = 3.141529;


const char *TAG = "mf_receiver";

/* MF tones */

const uint8_t MFT_700 = 0x20;
const uint8_t MFT_900 = 0x10;
const uint8_t MFT_1100 = 0x08;
const uint8_t MFT_1300 = 0x04;
const uint8_t MFT_1500 = 0x02;
const uint8_t MFT_1700 = 0x01;


/* MF Tone codes */

const uint8_t MFC_KP = (MFT_1100 | MFT_1700);
const uint8_t MFC_ST = (MFT_1500 | MFT_1700);
const uint8_t MFC_STP = (MFT_900 | MFT_1700);
const uint8_t MFC_ST2P = (MFT_1300 | MFT_1700);
const uint8_t MFC_ST3P = (MFT_700 | MFT_1700);
const uint8_t MFC_0 = (MFT_1300 | MFT_1500);
const uint8_t MFC_1 = (MFT_700 | MFT_900);
const uint8_t MFC_2 = (MFT_700 | MFT_1100);
const uint8_t MFC_3 = (MFT_900 | MFT_1100);
const uint8_t MFC_4 = (MFT_700 | MFT_1300);
const uint8_t MFC_5 = (MFT_900 | MFT_1300);
const uint8_t MFC_6 = (MFT_1100 | MFT_1300);
const uint8_t MFC_7 = (MFT_700 | MFT_1500);
const uint8_t MFC_8 = (MFT_900 | MFT_1500);
const uint8_t MFC_9 = (MFT_1100 | MFT_1500);


static const uint8_t mf_decode_table[MF_DECODE_TABLE_SIZE] = { MFC_0, MFC_1, MFC_2, MFC_3, MFC_4, MFC_5, MFC_6, MFC_7, MFC_8, MFC_9, MFC_KP, MFC_ST, MFC_STP, MFC_ST2P, MFC_ST3P};
static const char digit_map[MF_DECODE_TABLE_SIZE] =          { '0',   '1',   '2',   '3',   '4',   '5',   '6',   '7',   '8',   '9',   '*',    '#',    'A',     'B',      'C'  };


static const float frequencies[NUM_MF_FREQUENCIES] = {1700.0, 1500.0, 1300.0, 1100.0, 900.0, 700.0};

/* CCRAM usage */
static mfDataGoertzel _mf_data_goertzel[NUM_MF_RECEIVERS] __attribute__((section(".ccmram")));





/*
 * Worker thread
 */



static void _worker(void *args) {
	MF_decoder.receiver_worker(args); /* Workaround for non static function compile error */
}

void MF_Decoder::receiver_worker(void *args) {
	osStatus_t status;
	queueData qd;
	mfDataGoertzel *goertzel_data;
	uint16_t *dma_buffer;
	uint32_t descriptor;


	for(;;) {
		/* Wait for work */
		status = osMessageQueueGet(this->_message_queue, &qd, NULL, osWaitForever);
		if(status != osOK) {
			LOG_PANIC(TAG, "Message queue status returned %d", status);
		}

		/* UPDATE_SCOPE_TEST_POINT(SCOPE_TP2, (bool) qd.buffer_number); */
		/* UPDATE_SCOPE_TEST_POINT(SCOPE_TP1, true); */

		descriptor = qd.receiver;

		/* Point to the correct goertzel data block */
		goertzel_data = _mf_data_goertzel + descriptor;


		/* Point to the correct dma half-buffer */
		dma_buffer = (qd.buffer_number) ? this->_mf_data[descriptor].mf_dma_buffer +
				MF_FRAME_SIZE : this->_mf_data[descriptor].mf_dma_buffer;



		/* PASS 1: Convert to bipolar format, and record min and max values */
		/* This takes appx. 200 microseconds */

		float max = 1.0;
		float min = -1.0;
		for (int sample_index = 0; sample_index < MF_FRAME_SIZE; sample_index++) {
			/* center around 0 */
			float val = ((float) dma_buffer[sample_index]) + MIN_ADC;

			/* Scale to range -1 to 1 */
			val /= -MIN_ADC;

			if (val > max) {
				max = val;
			}
			if ( val < min ) {
				min = val;
			}
			/* Store converted float value in CCMRAM */
			goertzel_data->goertzel_block[sample_index] = val;
		}

		/* PASS 2: Use the min and max values to remove any DC offset */
		/* This takes appx. 50 microseconds */

		register float dc_offset = (((1.0 - max)) - (1.0 - fabs(min)));

		for (int sample_index = 0; sample_index < MF_FRAME_SIZE; sample_index++) {
			goertzel_data->goertzel_block[sample_index] -= dc_offset;
		}

		/* PASS 3: Decode MF tones */
		/* This takes appx. 1.17 milliseconds */


		/* Clear previous values for all goertzel tone decoders */
		for (int mf_freq_index = 0; mf_freq_index < NUM_MF_FREQUENCIES; mf_freq_index++) {
			goertzel_data->goertzel_data[mf_freq_index].q1 = goertzel_data->goertzel_data[mf_freq_index].q2 = 0.0;
		}

	    /* Go through the frame and calculate the goertzel data values */
		for (int sample_index = 0; sample_index < MF_FRAME_SIZE; sample_index++) {
			register float s = goertzel_data->goertzel_block[sample_index];
			for (int mf_freq_index = 0; mf_freq_index < NUM_MF_FREQUENCIES; mf_freq_index++) {
				float q0 = goertzel_data->goertzel_data[mf_freq_index].coeff_k * goertzel_data->goertzel_data[mf_freq_index].q1 -
						goertzel_data->goertzel_data[mf_freq_index].q2 + s;
				goertzel_data->goertzel_data[mf_freq_index].q2 = goertzel_data->goertzel_data[mf_freq_index].q1;
				goertzel_data->goertzel_data[mf_freq_index].q1 = q0;
			}
		}

		/* Calculate power from the goertzel data values */
		for (int mf_freq_index = 0; mf_freq_index < NUM_MF_FREQUENCIES; mf_freq_index++) {
			goertzel_data->goertzel_data[mf_freq_index].power = sqrt(goertzel_data->goertzel_data[mf_freq_index].q1 * goertzel_data->goertzel_data[mf_freq_index].q1 +
					goertzel_data->goertzel_data[mf_freq_index].q2 * goertzel_data->goertzel_data[mf_freq_index].q2 -
					goertzel_data->goertzel_data[mf_freq_index].coeff_k * goertzel_data->goertzel_data[mf_freq_index].q1 * goertzel_data->goertzel_data[mf_freq_index].q2);
		}

		/* End of pass 3 */

		/* Check for tones */
		bool silence = false;
		bool valid_code = false;

		uint8_t mf_code = 0;


	    /* Test to see if MF tone component is above the threshold */
		for(uint8_t tone_index = 0; tone_index < NUM_MF_FREQUENCIES; tone_index++) {

			if (goertzel_data->goertzel_data[tone_index].power > SILENCE_THRESHOLD) {
				mf_code |= (1 << tone_index);

			}
		}
		/* Count the number of tones present */
		uint8_t tones_present = 0;
		for (uint8_t tone_index = 0; tone_index < NUM_MF_FREQUENCIES; tone_index++) {
			if (mf_code & (1 << tone_index)) {
				tones_present++;
			}
		}

		/* Check for silence */
		if (tones_present == 0) {
			silence = true;
		}
		/* Check for exactly 2 tones */
		else if (tones_present == 2){
			valid_code = true;
		}

		/* Decoder state machine */
		osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

		/* Reference the state data */
		mfData *dp = &this->_mf_data[descriptor];

		switch(dp->state) {
			case MFR_IDLE:
				break;

			case MFR_WAIT_KP:
				if (!silence && valid_code == true && mf_code == MFC_KP) {
					if (dp->tone_block_count >= MIN_KP_GATE_BLOCK_COUNT){
						dp->digit_count = 0;
						dp->digits[0] = '*'; /* Add KP to string */
						dp->digit_count++;
						dp->timer = 0;
						dp->state = MFR_KP_SILENCE;
					}
					else {
						dp->tone_block_count++;
					}
				}
				break;

			case MFR_KP_SILENCE:
				if (silence) {
					dp->state = MFR_WAIT_DIGIT;
					dp->tone_block_count = 0;
					dp->timer = 0;
				}
				else {
					dp->timer++;
					if (dp->timer >= MF_INTERDIGIT_TIMEOUT) {
						dp->state = MFR_TIMEOUT;
					}
				}
				break;

			case MFR_WAIT_DIGIT:
				if (!silence && valid_code == true) {
					if (dp->tone_block_count >= MIN_DIGIT_BLOCK_COUNT - 1) {
						uint8_t tone_number;
						for (tone_number = 0; tone_number < MF_DECODE_TABLE_SIZE; tone_number++) {
							if (mf_code == mf_decode_table[tone_number]) {
								break;
							}
						}
						if (tone_number < MF_DECODE_TABLE_SIZE) {
							dp->tone_digit = digit_map[tone_number];
							dp->state = MFR_WAIT_DIGIT_SILENCE;
							dp->timer = 0;
						}
					}
					else {
						dp->tone_block_count++;
					}
				}
				else {
					dp->timer++;
					if (dp->timer >= MF_INTERDIGIT_TIMEOUT) {
						dp->state = MFR_TIMEOUT;
					}
				}
				break;

			case MFR_WAIT_DIGIT_SILENCE:
				if (silence) {
					if ((dp->tone_digit != '#') && /* If not an ST of some type */
							(dp->tone_digit != 'A') &&
							(dp->tone_digit != 'B') &&
							(dp->tone_digit != 'C')) {


						dp->tone_block_count = 0;
						dp->timer = 0;
						if(dp->digit_count < MF_MAX_DIGITS) {
							dp->digits[dp->digit_count++] = dp->tone_digit;
						}
						/* Wait for next digit */
						dp->state = MFR_WAIT_DIGIT;
					}
					else {
						/* Add the ST, STP, ST2P, or ST3P character to the end of the digit string */
						if (dp->digit_count < MF_MAX_DIGITS){
							dp->digits[dp->digit_count++] = dp->tone_digit;
						}
						/* Terminate the digit string */
						if (dp->digit_count < MF_MAX_DIGITS){
							dp->digits[dp->digit_count] = 0;
						}
						else {
							dp->digits[MF_MAX_DIGITS-1] = 0;
						}
						dp->state = MFR_DONE;
					}
				}
				else {
					dp->timer++;
					if (dp->timer >= MF_INTERDIGIT_TIMEOUT) {
						dp->state = MFR_TIMEOUT;
					}
				}
				break;

			case MFR_TIMEOUT:
				dp->error_code = MFE_TIMEOUT;
				dp->state = MFR_DONE;
				break;

			case MFR_DONE:
				/* Call the user's callback function */
				(*dp->callback)(dp->parameter, dp->error_code, dp->digit_count, dp->digits);
				if(dp->re_arm) {
					/* Receiver auto re-arm */
					dp->state = MFR_WAIT_KP;
					dp->error_code = MFE_OK;
					dp->tone_digit = false;
					dp->tone_block_count = 0;
				}
				else {
					dp->state = MFR_WAIT_RELEASE;
				}
				break;

			case MFR_WAIT_RELEASE:
				break;


			default:
				dp->state = MFR_DONE;
				break;



		}
		osMutexRelease(this->_lock); /* Release the lock */
		/* UPDATE_SCOPE_TEST_POINT(SCOPE_TP1, false); */




	}
	osThreadTerminate(NULL);
}

/*
 * Start DMA transfers for a specific decoder
 */

void MF_Decoder::_start_dma_transfers(uint32_t receiver_descriptor) {
	if(receiver_descriptor == 0) {
		HAL_ADC_Start_DMA(&hadc1, (uint32_t *) this->_mf_data[receiver_descriptor].mf_dma_buffer, MF_ADC_BUF_LEN);
	}
	else if(receiver_descriptor == 1) {
		HAL_ADC_Start_DMA(&hadc2, (uint32_t *) this->_mf_data[receiver_descriptor].mf_dma_buffer, MF_ADC_BUF_LEN);
	}
	else {
		LOG_PANIC(TAG, "Invalid descriptor passed in: %d", receiver_descriptor);
	}

}


/*
 * Stop DMA transfers for a specific decoder
 */

void MF_Decoder::_stop_dma_transfers(uint32_t receiver_descriptor) {
	if(receiver_descriptor == 0) {
		HAL_ADC_Stop_DMA(&hadc1);
	}
	else if (receiver_descriptor == 1) {
		HAL_ADC_Stop_DMA(&hadc2);
	}
	else {
		LOG_PANIC(TAG, "Invalid descriptor passed in: %d", receiver_descriptor);
	}
}




void MF_Decoder::setup() {

	/* Initialize the Goertzel Coefficient which stays the same between uses */
	/* Clear power values */
	for (int receiver = 0; receiver < NUM_MF_RECEIVERS; receiver++) {
		for (int i = 0; i < NUM_MF_FREQUENCIES; i++) {
			_mf_data_goertzel[receiver].goertzel_data[i].power = 0.0;
			_mf_data_goertzel[receiver].goertzel_data[i].coeff_k = 2.0 * cos((2.0 * PI * frequencies[i]) / MF_SAMPLE_RATE);
		}
	}
}

/*
 * Initialize the decoder. This gets called once the RTOS is tunning
 */

void MF_Decoder::init() {

	osStatus status;
	/* Mutex attributes */
	static const osMutexAttr_t mfd_mutex_attr = {
		"MFDecoderMutex",
		osMutexRecursive | osMutexPrioInherit,
		NULL,
		0U
	};

	/* Worker thread attributes */
	static const osThreadAttr_t worker_attr = {
			"MFWorkerThread",
			osThreadDetached,
			NULL,
			0,
			NULL,
			1024,
			osPriorityRealtime4,
			0,
			0
	};

	/* Create message queue used be interrupt to awake the worker task */

	this->_message_queue = osMessageQueueNew(NUM_MSG_QUEUE_OBJECTS, sizeof(queueData), NULL);
	if (this->_message_queue == NULL) {
		LOG_PANIC(TAG, "Could not create message queue");
	  }

	/* Create mutex to protect mf receiver data between tasks */


	this->_lock = osMutexNew(&mfd_mutex_attr);
	if (this->_lock == NULL) {
			LOG_PANIC(TAG, "Could not create lock");
		  }

	/* Initialize DMA timer for MF decoders */
	if ((status = HAL_TIM_OC_Start(&htim4, TIM_CHANNEL_1) != HAL_OK)) {
		LOG_PANIC(TAG, "Could not start timer 4 channel 1, RTOS status: ", status);
	}

	/* Create worker task */
	if(osThreadNew(_worker, NULL, &worker_attr) == NULL) {
		LOG_PANIC(TAG, "Could not start worker thread");
	}

}

/*
* Attempt to seize the MF receiver.
* Will return descriptor if successful.
* Will return -1 if no receiver is available
*/

int32_t MF_Decoder::seize(Mf_Callback callback, void *parameter, int channel, bool re_arm) {

	int32_t descriptor;


	if(!callback) {
		LOG_PANIC(TAG, "NULL passed in for callback");
	}

	/* Get the lock */
	osMutexAcquire(this->_lock, osWaitForever);

	if(channel == -1) {
		/* Auto select receiver */
		for(descriptor = 0; descriptor < NUM_MF_RECEIVERS; descriptor++) {
			/* Find an available receiver */
			if((this->_rx_in_use_bits & (1 << descriptor)) == 0) {
				this->_rx_in_use_bits |= (1 << descriptor);
				break;
			}
		}
	}
	else if((channel > 0) || (channel < NUM_MF_RECEIVERS)) {
		/* Manually select receiver */
		if((this->_rx_in_use_bits & (1 << channel)) == 0) {
			this->_rx_in_use_bits |= (1 << channel);
			descriptor = (int32_t) channel;
		}
		else {
			/* Receiver not available */
			descriptor = NUM_MF_RECEIVERS;
		}
	}
	else {
		LOG_PANIC(TAG, "Invalid MF receiver channel");
	}

	if(descriptor >= NUM_MF_RECEIVERS) {
		/* No receiver available */
		descriptor = -1;
	}
	else {
		/* Initialize the receiver */
		this->_mf_data[descriptor].re_arm = re_arm;
		this->_mf_data[descriptor].parameter = parameter;
		this->_mf_data[descriptor].error_code = MFE_OK;
		this->_mf_data[descriptor].callback = callback;
		this->_mf_data[descriptor].tone_digit = false;
		this->_mf_data[descriptor].digit_count = 0;
		this->_mf_data[descriptor].tone_block_count = 0;
		this->_mf_data[descriptor].timer = 0;
		this->_mf_data[descriptor].state = MFR_WAIT_KP;

		/* Start transferring data */
		this->_start_dma_transfers(descriptor);
	}
	/* Release the lock */
  	osMutexRelease(this->_lock);

  	return descriptor;
}


/*
* Release the MF receiver. Must be called outside of the callback or a deadlock will result.
*
* Returns true if successful
*/

void MF_Decoder::release(int32_t descriptor) {


	if((descriptor >= NUM_MF_RECEIVERS) || (descriptor < 0)) {
		LOG_PANIC(TAG, "Invalid descriptor passed in");
	}

	/* Get the lock */
	osMutexAcquire(this->_lock, osWaitForever);

	if(this->_rx_in_use_bits & (1 << descriptor)) {
		this->_rx_in_use_bits &= ~(1 << descriptor);

		/* De initialize MF receiver */
		this->_stop_dma_transfers(descriptor);
		this->_mf_data[descriptor].state = MFR_IDLE;


	}
	else {
		LOG_PANIC(TAG, "Attempt made to free MF receiver already available");
	}

	/* Release the lock */
	osMutexRelease(this->_lock);

}

/*
 * ISR for DMA buffer full and half full interrupts
 */

void MF_Decoder::handle_buffer(ADC_HandleTypeDef *hadc, uint8_t buffer_no) {
	osStatus_t status;
	queueData msg;
	/* Determine which descriptor needs to be processed */
	if(hadc == &hadc1) {
		msg.receiver = 0;
	}
	else if (hadc == &hadc2) {
		msg.receiver = 1;
	}
	else {
		return;
	}
	/* Add message number */
	msg.buffer_number = buffer_no;

	status = osMessageQueuePut(this->_message_queue, &msg, 0U, 0U);
	if(status != osOK) {
		LOG_ERROR(TAG, "Can't place interrupt event in queue, status = %d ", status);
	}
}

} // End Namespace MF_Decoder
