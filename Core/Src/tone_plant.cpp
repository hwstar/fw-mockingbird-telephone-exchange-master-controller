#include "top.h"
#include "util.h"
#include "tone_plant.h"
#include "logging.h"
#include <math.h>
#include <mf_receiver.h>

static const char *TAG = "toneplant";

namespace Tone_Plant {

#include "sine.h"

/* Channel buffers in CCMRAM for performance reasons */
static int16_t channel_buffers[NUM_TONE_OUTPUTS][CHANNEL_BUFFER_SIZE] __attribute__((section(".ccmram")));
/* Audio samples stored in CCRAM for throughput and RAM space utilization reasons */
static uint8_t audio_samples_buffer_pool[AUDIO_SAMPLE_BUFFER_POOL_SIZE] __attribute__((section(".ccmram")));

/*
 * Enable the TX dma for a given sai channel
 */

void Tone_Plant::_enable_dma(uint32_t sai_channel) {
	if(sai_channel >= NUM_SAI_CHANNELS) {
		LOG_PANIC(TAG, "Invalid SAI channel number");
	}
	if(sai_channel == 1) {
		HAL_SAI_Transmit_DMA(&hsai_BlockB1, (uint8_t *) this->_sai_data[1].dma_buffer, BUFFER_SIZE);
	}
	else {
		HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *) this->_sai_data[0].dma_buffer, BUFFER_SIZE);

	}

}

/*
 * Disable the TX dma for a given sai channel
 */

void Tone_Plant::_disable_dma(uint32_t sai_channel) {
	if(sai_channel >= 2) {
		LOG_PANIC(TAG, "Invalid SAI channel number");
	}

	if(sai_channel == 1) {
		HAL_SAI_DMAStop(&hsai_BlockB1);
	}
	else {
		HAL_SAI_DMAStop(&hsai_BlockA1);
	}


}

/*
 * Disable the tx mute
 */

void Tone_Plant::_disable_tx_mute(uint32_t sai_channel) {
	if(sai_channel >= NUM_SAI_CHANNELS) {
		LOG_PANIC(TAG, "Invalid SAI channel number");
	}
	if(sai_channel == 1) {
		HAL_SAI_DisableTxMuteMode(&hsai_BlockB1);
	}
	else {
		HAL_SAI_DisableTxMuteMode(&hsai_BlockA1);

	}

}


/*
 * Merge left and and right channel buffers into the correct dma buffer half
 */

void Tone_Plant::_merge_channel_buffers(queueData *qd) {

	register uint32_t buffer_base = 2 * qd->sai_number;
	register int16_t *left_channel = channel_buffers[buffer_base];
	register int16_t *right_channel = channel_buffers[buffer_base + RIGHT_OFFSET];
	register int16_t *dma_buffer = this->_sai_data[qd->sai_number].dma_buffer;
	register int16_t *dma_buffer_half = (qd->buffer_number) ? dma_buffer + HALF_BUFFER_SIZE : dma_buffer;



	for (uint32_t i = 0; i < HALF_BUFFER_SIZE; i+=16) {
		/* Partial loop unroll: do 8 groups of 2 per loop*/
		*dma_buffer_half++ = *left_channel++;
		*dma_buffer_half++ = *right_channel++;

		*dma_buffer_half++ = *left_channel++;
		*dma_buffer_half++ = *right_channel++;

		*dma_buffer_half++ = *left_channel++;
		*dma_buffer_half++ = *right_channel++;

		*dma_buffer_half++ = *left_channel++;
		*dma_buffer_half++ = *right_channel++;

		*dma_buffer_half++ = *left_channel++;
		*dma_buffer_half++ = *right_channel++;

		*dma_buffer_half++ = *left_channel++;
		*dma_buffer_half++ = *right_channel++;

		*dma_buffer_half++ = *left_channel++;
		*dma_buffer_half++ = *right_channel++;

		*dma_buffer_half++ = *left_channel++;
		*dma_buffer_half++ = *right_channel++;
	}

}

/*
 * Expand uLAW encoded byte to 13 bit signed linear
 */

int16_t Tone_Plant::_ulaw2slin13(uint8_t ulawbyte) {

	static int exp_lut[8]={0,132,396,924,1980,4092,8316,16764};

	int sign, exponent, mantissa, sample;
	ulawbyte = ~ulawbyte;
	sign = (ulawbyte & 0x80);
	exponent = (ulawbyte >> 4) & 0x07;
	mantissa = ulawbyte & 0x0F;
	sample = exp_lut[exponent] + (mantissa << (exponent+3));
	if (sign != 0) {
		sample = -sample;
	}
	return sample;
}

/*
 * Adjust the gain for the audio sample value
 */


int16_t Tone_Plant::_set_gain(channelInfo *channel_info, int16_t input_sample) {
	return (int16_t) input_sample * channel_info->audio_samples_level;
}


/*
 * Set up the generation of a single tone
 */

void Tone_Plant::_generate_tone(channelInfo *channel_info, float freq, float level) {
	this->_generate_dual_tone(channel_info, freq, 0.0, level, 0.0);
}
/*
 * Set up the generation of a dual tone
 */

void Tone_Plant::_generate_dual_tone(channelInfo *channel_info, float freq1, float freq2, float db_level1, float db_level2) {

	channel_info->f1 = freq1;
	channel_info->f2 = freq2;
	// Treat as DbV here.
	channel_info->f1_level = pow(10,(db_level1/20));
	channel_info->f2_level = pow(10,(db_level2/20));

	channel_info->phase_accum[0] = 0;
	channel_info->phase_accum[1] = 0;
	/*
	 *  Formula:
	 *
	 *  (2^N * Fout)/Fs
	 *  Where:
	 *  Fs = sample frequency
	 *  Fout is the desired output frequency
	 *  2^N is maximum value of the phase accumulator + 1
	 *
	 */
	channel_info->tuning_word[0] = (uint16_t) (((PHASE_ACCUM_MODULO_N) * ((float) channel_info->f1)) / ((float) SAMPLE_RATE));
	channel_info->tuning_word[1] = (uint16_t) (((PHASE_ACCUM_MODULO_N) * ((float) channel_info->f2)) / ((float) SAMPLE_RATE));
}

/*
 * Call to return the next computed tone value
 */

int16_t Tone_Plant::_next_tone_value(channelInfo *channel_info) {


	int16_t rawval_f1 = 0;
	int16_t rawval_f2 = 0;

	if(channel_info->f1 != 0.0) {
		uint16_t sine_table_index = (uint16_t) (channel_info->phase_accum[0] >> PHASE_ACCUMULATOR_TRUNCATION); /* Get sine table index for F1 */
		rawval_f1 = lut[sine_table_index];
		/* For signed audio, positive and negative excursions need to be handled differently */
		if(rawval_f1 >= 0){
			rawval_f1 = rawval_f1 * channel_info->f1_level;
		}
		else {
			rawval_f1 = -(-rawval_f1 * channel_info->f1_level);
		}
	}

	if(channel_info->f2 != 0.0) {
		uint16_t sine_table_index = (uint16_t) (channel_info->phase_accum[1] >> PHASE_ACCUMULATOR_TRUNCATION); /* Get sine table index for F2 */
		rawval_f2 = lut[sine_table_index];
		/* For signed audio, positive and negative excursions need to be handled differently */
		if(rawval_f2 >= 0){
			rawval_f2 = rawval_f2 * channel_info->f2_level;
		}
		else {
			rawval_f2 = -(-rawval_f2 * channel_info->f2_level);
		}
	}

	/* Advance to next phase accumulator value */
	for(int i = 0; i < 2; i++) {
		channel_info->phase_accum [i] = (channel_info->phase_accum[i] + channel_info->tuning_word[i]) & (PHASE_ACCUMULATOR_MASK);
	}

	return rawval_f1 + rawval_f2;
}

/*
 * Return a duration for an MF tone based on the digit
 */

uint32_t Tone_Plant::_get_mf_tone_duration(uint8_t mf_digit) {
	uint32_t duration_ms;
	switch(mf_digit) {
		case MF_KP:
			duration_ms = MF.kp_active_time_ms;
			break;

		case MF_ST:
		case MF_STP:
		case MF_ST2P:
		case MF_ST3P:
			duration_ms = MF.st_active_time_ms;
			break;

		default:
			duration_ms = MF.active_time_ms;
			break;
	}
	return this->_convert_ms(duration_ms);
}



/*
 * Convert ASCII tone string to binary representation
 */

bool Tone_Plant::_convert_digit_string(channelInfo *ch_info, const char *digits, bool is_mf) {
	uint8_t i, code;

	if(!ch_info || !digits) {
		LOG_PANIC(TAG, "Null argument(s) passed");
	}

	ch_info->digit_string_index = 0;
	ch_info->digit_string_length = 0;


	for(i = 0; i < strlen(digits); i++) {
		if(i >= DIGIT_STRING_MAX_LENGTH) {
			LOG_ERROR(TAG, "Digit string exceeds maximum length");
			return false;
		}
		if(digits[i] == 0) { /* End of digit string */
			break;
		}
		switch(digits[i]) {
			case '*':
				code = 0x0A;
				break;

			case '#':
				code = 0x0B;
				break;

			case 'A':
				code = 0x0C;
				break;

			case 'B':
				code = 0x0D;
				break;

			case 'C':
				code = 0x0E;
				break;

			case 'D':
				if(!is_mf) {
					code = 0x0F;
				}
				else {
					LOG_ERROR(TAG,"Invalid mf tone digit");
					return false;
				}
				break;

			default:
				/* Digits 0 through 9 */
				if((digits[i] >= '0') && (digits[i] <= '9')) {
					code = digits[i] - 0x30;
				}
				else {
					LOG_ERROR(TAG, "Invalid tone digit");
					return false;
				}
				break;
		}
		/* Store the code digit in the dial string */
		ch_info->digit_string[ch_info->digit_string_length++] = code;
	}
	return true;

}

/*
 * Validate a descriptor passed in by the caller
 */

bool Tone_Plant::_validate_descriptor(uint32_t descriptor) {
	if((descriptor >= 0) && (descriptor >= NUM_TONE_OUTPUTS)){
		return false;
	}
	return true;
}


/*
 * Worker thread
 */

static void _worker(void *args) {
	Tone_plant.worker(); /* Workaround to call class member from RTOS */
}

void Tone_Plant::worker(void) {
	osStatus_t status;
	queueData qd;

	for(;;) {

		/* Wait for work */

		status = osMessageQueueGet(this->_message_queue, &qd, NULL, osWaitForever);
		if(status != osOK) {
			LOG_PANIC(TAG, "Message queue status returned %d", status);
		}

		UPDATE_SCOPE_TEST_POINT(SCOPE_TP1, true);

		osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

		/* Iterate through two channels */
		for (uint32_t channel_num = 0; channel_num < 2; channel_num++) {
			int16_t *buffer = channel_buffers[(2 * qd.sai_number) + channel_num];
			channelInfo *ch_info = &this->_channel_info[(2* qd.sai_number) + channel_num];
			/* Iterate through channel buffer */
			for (uint32_t i = 0; i < CHANNEL_BUFFER_SIZE; i++) {
				buffer[i] = 0; /* Assume a zero value */


				/*
				 * State machine
				 */


				switch(ch_info->state) {

				case AS_IDLE:
					break;

				case AS_SEND_SINGLE_TONE:
					ch_info->is_stoppable = true;
					this->_generate_tone(ch_info, ch_info->test_tone_freq, ch_info->test_tone_level);
					ch_info->state = AS_SEND_SINGLE_TONE_WAIT;
					break;



				case AS_GEN_DIAL_TONE:
					ch_info->is_stoppable = true;
					this->_generate_dual_tone(ch_info,
						INDICATIONS.dial_tone.tone_pair[0], /* F1 */
						INDICATIONS.dial_tone.tone_pair[1], /* F2 */
						INDICATIONS.dial_tone.level_pair[0],/* L1 */
						INDICATIONS.dial_tone.level_pair[1] /* L2 */
					);
					ch_info->state = AS_GEN_DIAL_TONE_WAIT;
					break;

				case AS_GEN_DIAL_TONE_WAIT:
				case AS_SEND_SINGLE_TONE_WAIT:
					buffer[i] = this->_next_tone_value(ch_info);
					break;


				case AS_GEN_BUSY_TONE:
				case AS_GEN_CONGESTION_TONE:
					ch_info->is_stoppable = true;
					if (ch_info->state == AS_GEN_CONGESTION_TONE) {
						ch_info->cadence_timing = _convert_ms(INDICATIONS.busy.congestion_cadence_ms);
					}
					else {
						ch_info->cadence_timing =  _convert_ms(INDICATIONS.busy.busy_cadence_ms);
					}
					ch_info->cadence_timer = ch_info->cadence_timing;

					this->_generate_dual_tone(ch_info,
						INDICATIONS.busy.tone_pair[0], /* F1 */
						INDICATIONS.busy.tone_pair[1], /* F2 */
						INDICATIONS.busy.level_pair[0],/* L1 */
						INDICATIONS.busy.level_pair[1] /* L2 */
					);
					ch_info->state = AS_BUSY_WAIT_TONE_END;
					break;

				case AS_BUSY_WAIT_TONE_END:
					buffer[i] = this->_next_tone_value(ch_info);
					if(ch_info->cadence_timer == 0){
						if((buffer[i] > -TONE_SHUTOFF_THRESHOLD) && (buffer[i] < TONE_SHUTOFF_THRESHOLD)) { /* Shut off close to zero to reduce audio clicking. Changes the tone timing ever so slightly */
							ch_info->cadence_timer = ch_info->cadence_timing;
							ch_info->state = AS_BUSY_WAIT_SILENCE_END;
						}
					}
					else {
						ch_info->cadence_timer--;
					}

					break;

				case AS_BUSY_WAIT_SILENCE_END:
					if(ch_info->cadence_timer == 0){
						ch_info->cadence_timer = ch_info->cadence_timing;
						ch_info->state = AS_BUSY_WAIT_TONE_END;
					}
					else {
						ch_info->cadence_timer--;
					}

					break;


				case AS_GEN_RINGING_TONE:
					ch_info->is_stoppable = true;
					ch_info->cadence_timer =  _convert_ms(INDICATIONS.ringing.ring_on_cadence_ms);

					this->_generate_dual_tone(ch_info,
						INDICATIONS.ringing.tone_pair[0], /* F1 */
						INDICATIONS.ringing.tone_pair[1], /* F2 */
						INDICATIONS.ringing.level_pair[0],/* L1 */
						INDICATIONS.ringing.level_pair[1] /* L2 */
					);
					ch_info->state = AS_RINGING_WAIT_TONE_END;
					break;

				case AS_RINGING_WAIT_TONE_END:
					buffer[i] = this->_next_tone_value(ch_info);
					if(ch_info->cadence_timer == 0){
						if((buffer[i] > -TONE_SHUTOFF_THRESHOLD) && (buffer[i] < TONE_SHUTOFF_THRESHOLD)) { /* Shut off close to zero to reduce audio clicking. Changes the tone timing ever so slightly */
							ch_info->cadence_timer = _convert_ms(INDICATIONS.ringing.ring_off_cadence_ms);
							ch_info->state = AS_RINGING_WAIT_SILENCE_END;
						}
					}
					else {
						ch_info->cadence_timer--;
					}

					break;	ch_info->state = AS_IDLE;
					ch_info->state = AS_IDLE;

				case AS_RINGING_WAIT_SILENCE_END:
					if(ch_info->cadence_timer == 0){
						ch_info->cadence_timer = _convert_ms(INDICATIONS.ringing.ring_on_cadence_ms);
						ch_info->state = AS_RINGING_WAIT_TONE_END;
					}
					else {
						ch_info->cadence_timer--;
					}

					break;


				case AS_SEND_MF:
					ch_info->is_stoppable = false;
					if (!ch_info->digit_string_length) { /* Zero length aborts operation */
						ch_info->state = AS_IDLE;
					}
					else {
						/* First tone pair */
						ch_info->digit_string_index = 0;
						ch_info->cadence_timer = this->_get_mf_tone_duration(ch_info->digit_string[ch_info->digit_string_index]);
						this->_generate_dual_tone(ch_info,
										MF.tone_pairs[ch_info->digit_string[ch_info->digit_string_index]].low, /* F1 */
										MF.tone_pairs[ch_info->digit_string[ch_info->digit_string_index]].high, /* F2 */
										MF.levels.low,/* L1 */
										MF.levels.high /* L2 */
									);
						ch_info->digit_string_index++;
						ch_info->state = AS_SEND_MF_WAIT_TONE_END;
					}
					break;

				case AS_SEND_MF_WAIT_TONE_END:
					buffer[i] = this->_next_tone_value(ch_info);
					if (ch_info->cadence_timer == 0){
						if ((buffer[i] > -TONE_SHUTOFF_THRESHOLD) && (buffer[i] < TONE_SHUTOFF_THRESHOLD)) { /* Shut off close to zero to reduce audio clicking. Changes the tone timing ever so slightly */
							/* Test for end of tone sequence */
							if (ch_info->digit_string_index >= ch_info->digit_string_length) {
								/* Call the callback */
								ch_info->callback((i & 1) + 1);
								ch_info->state = AS_IDLE;
							}
							else {
								ch_info->cadence_timer = _convert_ms(MF.inactive_time_ms);
								ch_info->state = AS_SEND_MF_WAIT_SILENCE_END;
							}
						}
					}
					else {
						ch_info->cadence_timer--;
					}
					break;

				case AS_SEND_MF_WAIT_SILENCE_END:
					if (ch_info->cadence_timer == 0){
						/* Next tone pair */
						ch_info->cadence_timer = this->_get_mf_tone_duration(ch_info->digit_string[ch_info->digit_string_index]);
						this->_generate_dual_tone(ch_info,
										MF.tone_pairs[ch_info->digit_string[ch_info->digit_string_index]].low, /* F1 */
										MF.tone_pairs[ch_info->digit_string[ch_info->digit_string_index]].high, /* F2 */
										MF.levels.low,/* L1 */
										MF.levels.high /* L2 */
									);
						ch_info->digit_string_index++;
						ch_info->state = AS_SEND_MF_WAIT_TONE_END;
					}
					else {
						ch_info->cadence_timer--;
					}

				break;

				case AS_SEND_DTMF:
					ch_info->is_stoppable = false;
					if (!ch_info->digit_string_length) { /* Zero length aborts operation */
						ch_info->state = AS_IDLE;
					}
					else {
						/* First tone pair */
						ch_info->digit_string_index = 0;
						ch_info->cadence_timer = this->_convert_ms(DTMF.active_time_ms);
						this->_generate_dual_tone(ch_info,
										DTMF.tone_pairs[ch_info->digit_string[ch_info->digit_string_index]].low, /* F1 */
										DTMF.tone_pairs[ch_info->digit_string[ch_info->digit_string_index]].high, /* F2 */
										DTMF.levels.low,/* L1 */
										DTMF.levels.high /* L2 */
									);
						ch_info->digit_string_index++;
						ch_info->state = AS_SEND_DTMF_WAIT_TONE_END;
					}
					break;

				case AS_SEND_DTMF_WAIT_TONE_END:
					buffer[i] = this->_next_tone_value(ch_info);
					if (ch_info->cadence_timer == 0){
						if ((buffer[i] > -TONE_SHUTOFF_THRESHOLD) && (buffer[i] < TONE_SHUTOFF_THRESHOLD)) { /* Shut off close to zero to reduce audio clicking. Changes the tone timing ever so slightly */
							/* Test for end of tone sequence */
							if (ch_info->digit_string_index >= ch_info->digit_string_length) {
								/* Call the callback */
								ch_info->callback((i & 1) + 1);
								ch_info->state = AS_IDLE;
							}
							else {
								ch_info->cadence_timer = this->_convert_ms(DTMF.inactive_time_ms);
								ch_info->state = AS_SEND_DTMF_WAIT_SILENCE_END;
							}
						}
					}
					else {
						ch_info->cadence_timer--;
					}
					break;

				case AS_SEND_DTMF_WAIT_SILENCE_END:
					if (ch_info->cadence_timer == 0){
						/* Next tone pair */
						ch_info->cadence_timer = this->_convert_ms(DTMF.active_time_ms);
						this->_generate_dual_tone(ch_info,
										DTMF.tone_pairs[ch_info->digit_string[ch_info->digit_string_index]].low, /* F1 */
										DTMF.tone_pairs[ch_info->digit_string[ch_info->digit_string_index]].high, /* F2 */
										DTMF.levels.low,/* L1 */
										DTMF.levels.high /* L2 */
									);
						ch_info->digit_string_index++;
						ch_info->state = AS_SEND_DTMF_WAIT_TONE_END;
					}
					else {
						ch_info->cadence_timer--;
					}
					break;

				case AS_SEND_AUDIO_LOOP:
				case AS_SEND_AUDIO_LOOP_ULAW:
					ch_info->is_stoppable = true;
					ch_info->audio_sample_index = 0l;
					ch_info->state = (ch_info->state == AS_SEND_AUDIO_LOOP) ?
							AS_SEND_AUDIO_LOOP_WAIT :
							AS_SEND_AUDIO_LOOP_WAIT_ULAW;
					break;


				case AS_SEND_AUDIO_LOOP_WAIT:
				case AS_SEND_AUDIO_LOOP_WAIT_ULAW:
					if(ch_info->state == AS_SEND_AUDIO_LOOP_WAIT) {
						/* Signed linear */
						buffer[i] = this->_set_gain(ch_info, ch_info->audio_sample_halfwords[ch_info->audio_sample_index++]);
					}
					else {
						/* ULAW */
						buffer[i] = this->_set_gain(ch_info, this->_ulaw2slin13(ch_info->audio_sample_bytes[ch_info->audio_sample_index++]));


					}
					/* Keep sending the loop until we are stopped */
					if(ch_info->audio_sample_index >= ch_info->audio_sample_size) {
						ch_info->audio_sample_index = 0l;
					}
					break;

				case AS_SEND_AUDIO:
				case AS_SEND_AUDIO_ULAW:
					ch_info->is_stoppable = false;
					ch_info->audio_sample_index = 0l;
					ch_info->state = (ch_info->state == AS_SEND_AUDIO) ?
								AS_SEND_AUDIO_WAIT :
								AS_SEND_AUDIO_WAIT_ULAW;
					break;


				case AS_SEND_AUDIO_WAIT:
				case AS_SEND_AUDIO_WAIT_ULAW:
					if(ch_info->state == AS_SEND_AUDIO_WAIT) {
						/* Signed linear */
						buffer[i] = ch_info->audio_sample_halfwords[ch_info->audio_sample_index++];
					}
					else {
						/* ULAW */
						buffer[i] = this->_ulaw2slin13(ch_info->audio_sample_bytes[ch_info->audio_sample_index++]);

					}
					if(ch_info->audio_sample_index >= ch_info->audio_sample_size) {
						/* Call the callback */
						ch_info->callback((i & 1) + 1);
						ch_info->state = AS_IDLE;
					}
					break;

				default:
					_channel_info->state = AS_IDLE;
					break;

				}

			}
		}
		osMutexRelease(this->_lock); /* Release the lock */
		UPDATE_SCOPE_TEST_POINT(SCOPE_TP1, false);

		/* Merge the two channels as left/right interleaved */
		this->_merge_channel_buffers(&qd);


	}
	osThreadTerminate(NULL);

}

/*
 * Called when we have SAI DMA complete or half complete interrupt
 */

void Tone_Plant::handle_buffer(SAI_HandleTypeDef *hsai, uint32_t buffer_no) {
	queueData qd;
	osStatus_t status;
	register bool valid = false;
	qd.buffer_number = buffer_no;
	if(hsai == &hsai_BlockA1) {
		qd.sai_number = 0;
		valid = true;
	}
	else if (hsai == &hsai_BlockB1) {
		qd.sai_number = 1;
		valid = true;
	}
	if(valid) {
		status = osMessageQueuePut(this->_message_queue, &qd, 0U, 0U);
		if(status != osOK) {
			LOG_ERROR(TAG, "Could not place message in queue");
		}
	}
}

/*
 * Called when there was an SAI error
 */

void Tone_Plant::handle_error(SAI_HandleTypeDef *hsai) {
	uint32_t sai_number;
	if(hsai == &hsai_BlockA1) {
		sai_number = 0;

	}
	else {
		sai_number = 1;
	}

	this->_sai_errors[sai_number]++;

}

/*
 * Called once before RTOS is set up
 */
void Tone_Plant::setup(void) {

	/* Initialize variables, clear channel buffers */

	for (uint32_t channel = 0; channel < NUM_TONE_OUTPUTS; channel++) {
		for (uint32_t index = 0; index < CHANNEL_BUFFER_SIZE; index++) {
			channel_buffers[channel][index] = 0;
		}
	}
	this->_sai_errors[0] = 0;
	this->_sai_errors[1] = 0;

	this->_audio_buffer_info.bytes_available = AUDIO_SAMPLE_BUFFER_POOL_SIZE;
	this->_audio_buffer_info.next_buffer = audio_samples_buffer_pool;

}

/*
 * Called once by default RTOS task
 */


void Tone_Plant::init(void) {

	/* Mutex attributes */
	static const osMutexAttr_t tp_mutex_attr = {
		"TPDecoderMutex",
		osMutexRecursive | osMutexPrioInherit,
		NULL,
		0U
	};

	/* Worker thread attributes */
	static const osThreadAttr_t worker_attr = {
		"TPWorkerThread",
		osThreadDetached,
		NULL,
		0,
		NULL,
		1024,
		osPriorityRealtime3,
		0,
		0
	};

	/* Create message queue used be interrupt to awake the worker task */

	this->_message_queue = osMessageQueueNew(NUM_MSG_QUEUE_OBJECTS, sizeof(queueData), NULL);
	if (this->_message_queue == NULL) {
		LOG_PANIC(TAG, "Could not create message queue");
	  }

	/* Create mutex to protect tone plant data between tasks */


	this->_lock = osMutexNew(&tp_mutex_attr);
	if (this->_lock == NULL) {
			LOG_PANIC(TAG, "Could not create lock");
		  }

	/* Create worker task */
	if(osThreadNew(_worker, NULL, &worker_attr) == NULL) {
		LOG_PANIC(TAG, "Could not start worker thread");
	}

	/* Unmute TX audio channels */
	this->_disable_tx_mute(0);
	this->_disable_tx_mute(1);

	/* Enable DMA */
	this->_enable_dma(0);
	this->_enable_dma(1);

}


/*
 * Send call progress tones.
 * Will continue to call send progress tones until stop() is called.
 */
void Tone_Plant::send_call_progress_tones(uint32_t descriptor, uint8_t type) {

	if(type >= CPT_MAX){
		LOG_PANIC(TAG, "Invalid call progress type");
	}
	if(!this->_validate_descriptor(descriptor)) {
		LOG_PANIC(TAG, "Invalid descriptor");
	}

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	channelInfo *ch_info = &this->_channel_info[descriptor];

	/* Set the call progress tone type */
	switch(type) {
		case CPT_DIAL_TONE:
			ch_info->state = AS_GEN_DIAL_TONE;
			break;

		case CPT_BUSY:
			ch_info->state = AS_GEN_BUSY_TONE;
			break;

		case CPT_CONGESTION:
			ch_info->state = AS_GEN_CONGESTION_TONE;
			break;

		case CPT_RINGING:
			ch_info->state = AS_GEN_RINGING_TONE;
			break;
	}

	osMutexRelease(this->_lock); /* Release the lock */

}

/*
 * Send a set of digits using MF tones.
 * Call the callback function when the all the digits are sent
 */
void Tone_Plant::send_mf(int32_t descriptor, const char *digit_string, void (*callback)(uint32_t channel_number)) {

	if(!this->_validate_descriptor(descriptor)) {
		LOG_PANIC(TAG, "Invalid descriptor");
	}

	if((!digit_string) || (!callback)) {
		LOG_PANIC(TAG, "Invalid parameters");
	}

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */
	channelInfo *ch_info = &this->_channel_info[descriptor];

	int len = strlen(digit_string);
	ch_info->digit_string_length = 0;
	for (int i = 0; i < len; i++) {
		switch (digit_string[i]) {
			case 0:
				break;

			case '*':
				ch_info->digit_string[i] = 0x0a;
				ch_info->digit_string_length++;
				break;

			case '#':
				ch_info->digit_string[i] = 0x0b;
				ch_info->digit_string_length++;
				break;

			case 'A':
				ch_info->digit_string[i] = 0x0c;
				ch_info->digit_string_length++;
				break;

			case 'B':
				ch_info->digit_string[i] = 0x0d;
				ch_info->digit_string_length++;
				break;

			case 'C':
				ch_info->digit_string[i] = 0x0e;
				ch_info->digit_string_length++;
				break;

			default:
				if((digit_string[i] >= '0' || digit_string[i] <= '9')){
					ch_info->digit_string[i] = digit_string[i] - 0x30;
					ch_info->digit_string_length++;
				}
				break;
		}
		if(!digit_string[i])
			break;
	}



	ch_info->callback = callback;
	ch_info->state = AS_SEND_MF;

	osMutexRelease(this->_lock); /* Release the lock */


}

/*
 * Send a set of digis using DTMF tones.
 * Call the callback function when the all the digits are sent
 */
void Tone_Plant::send_dtmf(int32_t descriptor, const char *digit_string, void (*callback)(uint32_t channel_number)) {
	if(!this->_validate_descriptor(descriptor)) {
		LOG_PANIC(TAG, "Invalid descriptor");
	}

	if((!digit_string) || (!callback)) {
		LOG_PANIC(TAG, "Invalid parameters");
	}


	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */
	channelInfo *ch_info = &this->_channel_info[descriptor];

	int len = strlen(digit_string);
	ch_info->digit_string_length = 0;
	for (int i = 0; i < len; i++) {
		switch (digit_string[i]) {
			case 0:
				break;

			case '*':
				ch_info->digit_string[i] = 0x0a;
				ch_info->digit_string_length++;
				break;

			case '#':
				ch_info->digit_string[i] = 0x0b;
				ch_info->digit_string_length++;
				break;

			case 'A':
				ch_info->digit_string[i] = 0x0c;
				ch_info->digit_string_length++;
				break;

			case 'B':
				ch_info->digit_string[i] = 0x0d;
				ch_info->digit_string_length++;
				break;

			case 'C':
				ch_info->digit_string[i] = 0x0e;
				ch_info->digit_string_length++;
				break;

			case 'D':
				ch_info->digit_string[i] = 0x0f;
				ch_info->digit_string_length++;
				break;

			default:
				if((digit_string[i] >= '0' || digit_string[i] <= '9')){
					ch_info->digit_string[i] = digit_string[i] - 0x30;
					ch_info->digit_string_length++;
				}
				break;
		}
		if(!digit_string[i])
			break;
	}
	ch_info->callback = callback;
	ch_info->state = AS_SEND_DTMF;

	osMutexRelease(this->_lock); /* Release the lock */
}

/*
 * Send a single tone frequency
 */

void Tone_Plant::send_single_tone(uint32_t descriptor, float freq, float level) {
	if(!this->_validate_descriptor(descriptor)) {
			LOG_PANIC(TAG, "Invalid descriptor");
		}

	if(freq < 0.0 || freq > 3400.0 || level < -40.0 || level > 0.0) {
		LOG_PANIC(TAG, "Invalid parameter");

	}

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	channelInfo *ch_info = &this->_channel_info[descriptor];

	ch_info->test_tone_freq = freq;
	ch_info->test_tone_level = level;
	ch_info->state = AS_SEND_SINGLE_TONE;

	osMutexRelease(this->_lock); /* Release the lock */
}


/*
 * Send a signed linear audio sample
 * Call the callback function when the sample is completely sent
 */

void Tone_Plant::send(int32_t descriptor, const int16_t *samples,
	uint32_t length, void (*callback)(uint32_t channel_number), float level) {

	if (!this->_validate_descriptor(descriptor)) {
		LOG_PANIC(TAG, "Invalid descriptor");
	}

	if ((!samples) || (!callback)) {
		LOG_PANIC(TAG, "Invalid parameters");
	}

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */


	channelInfo *ch_info = &this->_channel_info[descriptor];

	ch_info->audio_samples_level = pow(10,(level/20));
	ch_info->callback = callback;
	ch_info->audio_sample_size = length;
	ch_info->audio_sample_halfwords = samples;
	ch_info->state = AS_SEND_AUDIO;

	osMutexRelease(this->_lock); /* Release the lock */
}

/*
 * Send a ulaw audio sample
 * Call the callback function when the sample is completely sent
 */

void Tone_Plant::send_ulaw(int32_t descriptor, const uint8_t *samples, uint32_t length,
	void (*callback)(uint32_t channel_number), float level) {

	if (!this->_validate_descriptor(descriptor)) {
		LOG_PANIC(TAG, "Invalid descriptor");
	}

	if ((!samples) || (!callback)) {
		LOG_PANIC(TAG, "Invalid parameters");
	}

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	channelInfo *ch_info = &this->_channel_info[descriptor];

	ch_info->audio_samples_level = pow(10,(level/20));
	ch_info->callback = callback;
	ch_info->audio_sample_size = length;
	ch_info->audio_sample_bytes = samples;
	ch_info->state = AS_SEND_AUDIO_ULAW;

	osMutexRelease(this->_lock); /* Release the lock */


}

/*
 * Send a ulaw audio sample from a named buffer
 * Call the callback function when the sample is completely sent
 *
 * Returns true if buffer found and audio transmission initiated.
 */

bool Tone_Plant::send_buffer_ulaw(int32_t descriptor,
	const char *buffer_name, void (*callback)(uint32_t channel_number), float level) {

	uint32_t size;
	uint8_t *buffer = this->get_audio_buffer(buffer_name, &size);
	if(!buffer) {
		return false;
	}
	this->send_ulaw(descriptor, buffer, size, callback, level);

	return true;
}


/*
 * Send a signed linear audio loop.
 *
 * Will continue to send the audio loop until the stop function is called.
 */

void Tone_Plant::send_loop(int32_t descriptor, const int16_t *samples, uint32_t length, float level) {

	if (!this->_validate_descriptor(descriptor)) {
		LOG_PANIC(TAG, "Invalid descriptor");
	}

	if (!samples) {
		LOG_PANIC(TAG, "Invalid parameters");
	}


	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */
	channelInfo *ch_info = &this->_channel_info[descriptor];

	ch_info->audio_samples_level = pow(10,(level/20));
	ch_info->audio_sample_halfwords = samples;
	ch_info->audio_sample_size = length;
	ch_info->state = AS_SEND_AUDIO_LOOP;

	osMutexRelease(this->_lock); /* Release the lock */

}

/*
 * Send a ulaw audio loop.
 *
 * Will continue to send the audio loop until the stop function is called.
 */

void Tone_Plant::send_loop_ulaw(int32_t descriptor, const uint8_t *samples, uint32_t length, float level) {

	if (!this->_validate_descriptor(descriptor)) {
		LOG_PANIC(TAG, "Invalid descriptor");
	}

	if (!samples) {
		LOG_PANIC(TAG, "Invalid parameters");
	}


	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */
	channelInfo *ch_info = &this->_channel_info[descriptor];

	ch_info->audio_samples_level = pow(10,(level/20));
	ch_info->audio_sample_size = length;
	ch_info->audio_sample_bytes = samples;
	ch_info->state = AS_SEND_AUDIO_LOOP_ULAW;

	osMutexRelease(this->_lock); /* Release the lock */
}



/*
 * Send a ulaw audio sample as a loop from a named buffer
 *
 * Returns true if buffer found and audio transmission initiated.
 */


bool Tone_Plant::send_buffer_loop_ulaw(int32_t descriptor, const char *buffer_name, float level) {

	uint32_t size;
	uint8_t *buffer = this->get_audio_buffer(buffer_name, &size);
	if(!buffer) {
		return false;
	}

	this->send_loop_ulaw(descriptor, buffer, size, level);

	return true;
}

/*
 * Stop call progress tones and audio loops from playing
 */

void Tone_Plant::stop(int32_t descriptor) {
	if(!this->_validate_descriptor(descriptor)) {
		LOG_PANIC(TAG, "Invalid descriptor");
	}
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	channelInfo *ch_info = &this->_channel_info[descriptor];
	ch_info->state = AS_IDLE;

	osMutexRelease(this->_lock); /* Release the lock */
}



/*
 * Seize an audio channel and return a descriptor.
 *
 * If the requested channel is -1, the next available channel will be returned,
 * else the requested channel will be tested to see if it is available.
 *
 * If no channel is available, return -1.
 */

int32_t Tone_Plant::channel_seize(int32_t requested_channel) {
	int32_t descriptor = (int32_t) NUM_TONE_OUTPUTS;;
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	if(requested_channel == -1) {
		for(descriptor = 0; descriptor < (int32_t) NUM_TONE_OUTPUTS; descriptor++){
			if((this->_busy_bits & (1 << descriptor)) == 0) {
				this->_busy_bits |= (1 << descriptor);
				break;
			}
		}
	}
	else { /* Test and seize a specific descriptor */
		if((requested_channel >= 0) && (requested_channel < (int32_t) NUM_TONE_OUTPUTS) &&
				((this->_busy_bits & (1 << requested_channel)) == 0)) {
			this->_busy_bits |= (1 << requested_channel);
			descriptor = requested_channel; /* Grant channel */
		}
		else {
			descriptor = (int32_t) NUM_TONE_OUTPUTS; /* Not available */
		}
	}

	osMutexRelease(this->_lock); /* Release the lock */
	if(descriptor >= (int32_t) NUM_TONE_OUTPUTS) {
		descriptor = -1;
	}
	return descriptor;
}

/*
 * Release an audio channel
 *
 * Return true if successful
 */


void Tone_Plant::channel_release(int32_t descriptor) {

	if(!this->_validate_descriptor(descriptor)){
		LOG_PANIC(TAG, "Invalid descriptor");
	}

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

    /* Stop any tones playing */
	channelInfo *ch_info = &this->_channel_info[descriptor];
	ch_info->state = AS_IDLE;

	/* Un-busy the channel */
	this->_busy_bits &= ~(1 << descriptor);

	osMutexRelease(this->_lock); /* Release the lock */

}

/*
 * Allocate an audio buffer from the audio buffer pool
 *
 * This is used at initialization when the audio samples are loaded
 * from the storage device.
 *
 * A buffer size and buffer name are passed in.
 *
 * If the allocation is successful a pointer to the buffer
 * will be returned.
 *
 * NULL will be returned on allocation failure.
 */


uint8_t *Tone_Plant::allocate_audio_buffer(uint32_t size, const char *name) {
	if((!name) || (size <= 0) || (this->_audio_buffer_info.num_buffers_allocated >= AUDIO_BUFFERS_MAX)) {
		return NULL;
	}
	if(size > this->_audio_buffer_info.bytes_available) {
		return NULL;
	}
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */
	/* Create pointer to our audio buffer entry for easier reference */
	audioBufferEntry *pabe = &this->_audio_buffer_entries[this->_audio_buffer_info.num_buffers_allocated];

	/* Copy buffer name */
	Utility.strncpy_term(pabe->name, name, AUDIO_BUFFER_ENTRY_NAME_SIZE);

	/* Set buffer start and size */
	pabe->buffer_start = this->_audio_buffer_info.next_buffer;
	pabe->buffer_size = size;

	/* Housekeeping */

	this->_audio_buffer_info.next_buffer += size;
	this->_audio_buffer_info.bytes_available -= size;
	this->_audio_buffer_info.num_buffers_allocated++;
	osMutexRelease(this->_lock); /* Release the lock */

	return pabe->buffer_start;
}


/*
 * Return buffer address if sample name is loaded in the buffer
 * else NULL if it isn't.
 *
 * If a pointer to the optional uin32_t size is passed in, it will be filled in with the
 * size of the allocated buffer if the buffer exists.
 */
uint8_t  *Tone_Plant::get_audio_buffer(const char *name, uint32_t *size) {
	uint8_t index;

	if(!name) {
		return NULL;
	}

	/* Attempt to locate buffer by name */
	for(index = 0; index < AUDIO_BUFFERS_MAX; index++) {
		if(!strcmp(name, this->_audio_buffer_entries[index].name)) {
			break;
		}
	}

	if(index >= AUDIO_BUFFERS_MAX) {
		/* Name not found */
		return NULL;
	}

	/* If caller wants the size */
	if(size) {
		*size = this->_audio_buffer_entries[index].buffer_size;
	}

	return this->_audio_buffer_entries[index].buffer_start;
}




} /* End Namespace Tone_Plant */

Tone_Plant::Tone_Plant Tone_plant;
