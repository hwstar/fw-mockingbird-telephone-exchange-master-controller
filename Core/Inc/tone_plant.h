#pragma once
#include "top.h"


namespace Tone_Plant {

enum {AS_IDLE=0,
	AS_SEND_SINGLE_TONE, AS_SEND_SINGLE_TONE_WAIT, AS_GEN_DIAL_TONE, AS_GEN_DIAL_TONE_WAIT,
	AS_GEN_BUSY_TONE, AS_GEN_CONGESTION_TONE, AS_BUSY_WAIT_TONE_END, AS_BUSY_WAIT_SILENCE_END,
	AS_GEN_RINGING_TONE, AS_RINGING_WAIT_TONE_END, AS_RINGING_WAIT_SILENCE_END,
	AS_SEND_MF, AS_SEND_MF_WAIT_TONE_END, AS_SEND_MF_WAIT_SILENCE_END,
	AS_SEND_DTMF, AS_SEND_DTMF_WAIT_TONE_END, AS_SEND_DTMF_WAIT_SILENCE_END,
	AS_SEND_AUDIO, AS_SEND_AUDIO_WAIT, AS_SEND_AUDIO_LOOP, AS_SEND_AUDIO_LOOP_WAIT,
	AS_SEND_AUDIO_ULAW, AS_SEND_AUDIO_WAIT_ULAW, AS_SEND_AUDIO_LOOP_ULAW, AS_SEND_AUDIO_LOOP_WAIT_ULAW
};

enum {CPT_DIAL_TONE=0, CPT_BUSY, CPT_CONGESTION, CPT_RINGING, CPT_MAX};

/*
 * Constants
 */


/* Misc */
const uint32_t LEFT_OFFSET = 0;
const uint32_t RIGHT_OFFSET = 1;
const uint32_t NUM_SAI_CHANNELS = 2;
const uint32_t SAMPLE_RATE = 8000; /* Actual is 8007 Hz due to a non precise clock, but 8007Hz will cause distortion */
const uint32_t CHANNEL_BUFFER_SIZE = 160; /*  20 mS Frame at 8 kHz */
const uint32_t SINE_TABLE_LENGTH = 1024;
const uint16_t SINE_TABLE_BIT_WIDTH = 10; /* 1KB of sine table */
const uint16_t PHASE_ACCUMULATOR_WIDTH = 16; /* 16 bits gives appx. 0.14 Hz of frequency resolution */
const uint16_t LR_AUDIO_BUFFER_SIZE = 320; /* Left and right audio buffer audio samples for a 20mS frame of both */
const int16_t TONE_SHUTOFF_THRESHOLD = 200; /* Adjustment to trade off clicking at the end of a tone, vs. the length of the tone */
const uint8_t DIGIT_STRING_MAX_LENGTH = 20;
const uint16_t NUM_MF_TONE_PAIRS = 15;
const uint16_t NUM_DTMF_TONE_PAIRS = 16;
const uint8_t MAX_TONES = 2;

/* Tone generation */
const uint32_t NUM_TONE_OUTPUTS = 2 * NUM_SAI_CHANNELS; /* Each SAI has a left and right channel */
const uint32_t NUM_MSG_QUEUE_OBJECTS = NUM_TONE_OUTPUTS * 2;
const uint32_t HALF_BUFFER_SIZE = CHANNEL_BUFFER_SIZE * 2; /* Left and right data interleaved*/
const uint32_t BUFFER_SIZE = 2 * HALF_BUFFER_SIZE;
const uint16_t PHASE_ACCUMULATOR_TRUNCATION = (PHASE_ACCUMULATOR_WIDTH - SINE_TABLE_BIT_WIDTH);
const uint32_t PHASE_ACCUM_MODULO_N = (1 << PHASE_ACCUMULATOR_WIDTH);
const uint32_t PHASE_ACCUMULATOR_MASK = (PHASE_ACCUM_MODULO_N - 1);
const uint32_t TIME_PER_SAMPLE_US = 1000000UL/SAMPLE_RATE;

/* Audio buffers */
const uint32_t AUDIO_SAMPLE_BUFFER_POOL_SIZE = 100L * 1024L;
const uint32_t AUDIO_BUFFER_ENTRY_NAME_SIZE = 32;
const uint8_t AUDIO_BUFFERS_MAX = 8;

/*
 * Data structures
 */

typedef struct audioBufferInfo {
	uint32_t num_buffers_allocated;
	uint32_t bytes_available;
	uint8_t *next_buffer;
} audioBufferInfo;

/* List entry for Audio Buffer */
typedef struct audioBufferEntry {
	char name[AUDIO_BUFFER_ENTRY_NAME_SIZE];
	uint8_t *buffer_start;
	uint32_t buffer_size;
} audioBufferEntry;

/* Data passed in message queue from interrupt */
typedef struct queueData {
	uint32_t buffer_number;
	uint32_t sai_number;
}queueData;

/* Data for a specific SAI */
typedef struct saiData {
	int16_t dma_buffer[BUFFER_SIZE];
}saiData;


/* Channel-specific data */

typedef struct channelInfo {
	bool is_stoppable;
	void (*callback)(uint32_t descriptor);
	uint8_t state;
	uint8_t digit_string[DIGIT_STRING_MAX_LENGTH];
	float f1;
	float f2;
	float f1_level;
	float f2_level;
	float test_tone_freq;
	float test_tone_level;
	float audio_samples_level;
	uint32_t cadence_timing;
	uint32_t cadence_timer;
	uint32_t phase_accum[MAX_TONES];
	uint16_t tuning_word[MAX_TONES];
	uint32_t audio_sample_size;
	uint32_t audio_sample_index;
	size_t digit_string_length;
	size_t digit_string_index;
	const int16_t *audio_sample_halfwords;
	const uint8_t *audio_sample_bytes;

} channelInfo;

typedef struct Indications {
	struct Dial_Tone {
		float tone_pair[2];
		float level_pair[2];
	} dial_tone;
	struct Busy {
		float tone_pair[2];
		float level_pair[2];
		uint16_t busy_cadence_ms;
		uint16_t congestion_cadence_ms;
	} busy;
	struct Ringing {
		float tone_pair[2];
		float level_pair[2];
		uint16_t ring_on_cadence_ms;
		uint16_t ring_off_cadence_ms;
	}ringing;
}Indications;

typedef struct Dtmf {
	struct Levels {
		float high;
		float low;
	}levels;
	struct Tone_Pairs {
		float high;
		float low;
	}tone_pairs[NUM_DTMF_TONE_PAIRS];
	int16_t active_time_ms;
	int16_t inactive_time_ms;
}Dtmf;


typedef struct Mf {
	struct Levels {
		float high;
		float low;
	}levels;
	struct Tone_Pairs {
		float high;
		float low;
	}tone_pairs[NUM_MF_TONE_PAIRS];
	int16_t kp_active_time_ms;
	int16_t active_time_ms;
	int16_t inactive_time_ms;
	int16_t st_active_time_ms;
}Mf;

/*
 * Constants: Initialized data structures
 */

const Indications INDICATIONS = { /* Follows the precise tone plan: https://en.wikipedia.org/wiki/Precise_tone_plan */
									{
											{350.0, 440.0}, /* Dial Tone Frequencies */
											{-7, -7}, /* Levels in dB */
									},
									{
											{480.0, 620.0}, /* Busy/Congestion Frequencies */
											{-7, -7}, /* Levels in dB*/
											500, /* Busy cadence */
											250 /* Circuit busy cadence */
									},
									{
											{440.0, 480.0}, /* Ringing Frequencies */
											{-7, -7}, /* Levels in dB*/
											2000, /* On cadence cadence */
											4000 /* Off cadence */
									}
};


const Dtmf DTMF = {
					{-8.0,-6.0}, /* Levels */
					{
							{941.0,1336.0}, /* 0 */
							{697.0,1209.0}, /* 1 */
							{697.0,1336.0}, /* 2 */
							{697.0,1477.0}, /* 3 */
							{770.0, 1209.0}, /* 4 */
							{770.0, 1336.0}, /* 5 */
							{770.0, 1477.0}, /* 6 */
							{852.0, 1209.0}, /* 7 */
							{852.0, 1336.0}, /* 8 */
							{852.0, 1477.0}, /* 9 */
							{941.0, 1209.0}, /* * */
							{941.0, 1477.0}, /* # */
							{697.0, 1633.0}, /* A */
							{770.0, 1633.0}, /* B */
							{852.0, 1633.0}, /* C */
							{941.0, 1633.0}, /* D */
					},
					50, /* Active time */
					50, /* Inactive time */
};

const Mf MF = {
					{-7.0, -7.0}, /* Levels */
					{
							{1300.0, 1500.0}, /* 0 */
							{700.0, 900.0}, /* 1 */
							{700.0, 1100.0}, /* 2 */
							{900.0, 1100.0}, /* 3 */
							{700.0, 1300.0}, /* 4 */
							{900.0, 1300.0}, /* 5 */
							{1100.0, 1300.0}, /* 6 */
							{700.0, 1500.0}, /* 7 */
							{900.0, 1500.0}, /* 8 */
							{1100.0, 1500.0}, /* 9 */
							{1100.0, 1700.0}, /* KP */
							{1500.0, 1700.0}, /* ST */
							{900.0, 1700.0}, /* STP */
							{1300.0, 1700.0}, /* ST2P */
							{700.0, 1700.0} /* ST3P */
					},
					110, /* KP active time */
					70, /* Digits 0-9 active time */
					70, /* Digit inactive time */
					70 /* ST digits inactive time */
};

/*
 * Class definitions
 */

class Tone_Plant {
public:
	void worker(void) __attribute__((section(".xccmram")));
	void handle_buffer(SAI_HandleTypeDef *hsai, uint32_t buffer_no) __attribute__((section(".xccmram")));
	void handle_error(SAI_HandleTypeDef *hsai);
	void setup(void);
	void init(void);
	void send_call_progress_tones(uint32_t channel_number, uint8_t type);
	void send_mf(int32_t descriptor, const char *digit_string, void (*callback)(uint32_t channel_number));
	void send_dtmf(int32_t descriptor, const char *digit_string, void (*callback)(uint32_t channel_number));
	void send(int32_t descriptor, const int16_t *samples, uint32_t length, void (*callback)(uint32_t channel_number), float level = 0.0);
	void send_ulaw(int32_t descriptor, const uint8_t *samples, uint32_t length, void (*callback)(uint32_t channel_number), float level = 0.0);
	bool send_buffer_ulaw(int32_t descriptor, const char *buffer_name, void (*callback)(uint32_t channel_number), float level = 0.0);
	void send_loop(int32_t descriptor, const int16_t *samples, uint32_t length, float level = 0.0);
	void send_loop_ulaw(int32_t descriptor, const uint8_t *samples, uint32_t length, float level = 0.0);
	bool send_buffer_loop_ulaw(int32_t descriptor, const char *buffer_name, float level = 0.0);
	void send_single_tone(uint32_t descriptor, float freq, float level);
	void stop(int32_t descriptor);
	int32_t channel_seize(int32_t requested_channel = -1);
	void channel_release(int32_t descriptor);
	uint8_t *allocate_audio_buffer(uint32_t size, const char *name);
	uint8_t *get_audio_buffer(const char *name, uint32_t *size = NULL);




protected:
	void _enable_dma(uint32_t sai_channel);
	void _disable_dma(uint32_t sai_channel);
	void _disable_tx_mute(uint32_t sai_channel);
	void _merge_channel_buffers(queueData *qd) __attribute__((section(".xccmram")));
	int16_t _ulaw2slin13(uint8_t ulawbyte);
	int16_t _set_gain(channelInfo *channel_info, int16_t input_sample);
	int32_t inline _convert_ms(uint16_t ms) { return ((((uint32_t) ms) * 1000)/TIME_PER_SAMPLE_US); }
	bool _convert_digit_string(channelInfo *ch_info, const char *digits, bool is_mf);
	bool _validate_descriptor(uint32_t descriptor);
	void _generate_tone(channelInfo *channel_info, float freq, float level);
	void _generate_dual_tone(channelInfo *channel_info, float freq1, float freq2, float db_level1, float db_level2);
	int16_t _next_tone_value(channelInfo *channel_info);
	uint32_t _get_mf_tone_duration(uint8_t mf_digit);



	uint16_t _busy_bits;
	channelInfo _channel_info[NUM_TONE_OUTPUTS];
	osMessageQueueId_t _message_queue;
	osMutexId_t _lock;
	saiData _sai_data[NUM_SAI_CHANNELS];
	uint32_t _sai_errors[NUM_SAI_CHANNELS];
	audioBufferInfo _audio_buffer_info;
	audioBufferEntry _audio_buffer_entries[AUDIO_BUFFERS_MAX];
};

} /* End namespace tone plant */

extern Tone_Plant::Tone_Plant Tone_plant;
