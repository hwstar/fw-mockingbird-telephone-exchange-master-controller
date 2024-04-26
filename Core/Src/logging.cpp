#include "top.h"
#include "logging.h"
#include "util.h"

namespace LOGGING {

static const char *TAG = "logger";

const char *log_level_strings[MAX_LOG_LEVEL] = {
	"PANIC",
    "ERROR",
    "WARN",
    "NOTICE",
    "INFO",
    "DEBUG"
};


/* Trick to call c++ method Logging::loop*/
static void _worker(void *args) {
	while(true) {
		Logger.loop();
		osDelay(1);


	}
	osThreadTerminate(NULL);

}


/*
 * Transmit a log item over the serial port
 */

void Logging::_xmit_logitem(const char *tag, uint8_t level, uint32_t timestamp, const char *str, uint32_t line) {
    if(level > MAX_LOG_LEVEL) {
        level = 0; /* Protect against bad log level being passed in. */
    }

    /* Convert time stamp to H:M:S.MS format */
    uint32_t hours = timestamp / 3600000;
    timestamp %= 36000000;
    uint8_t minutes = timestamp / 60000;
    timestamp %= 60000;
    uint8_t seconds = timestamp / 1000;
    timestamp %= 1000;

    printf("[%lu:%02u:%02u.%03lu] LOG_%s(%s.%ld):%s\n", hours, minutes, seconds, timestamp, log_level_strings[level], tag, line, str);
}


/*
 * Print panic message and shutdown
 */

void Logging::panic(const char *tag, uint32_t line, const char *format, ...) {
	va_list alp;
	va_start(alp, format);

	char log_message[MAX_LOG_SIZE];
	vsnprintf(log_message, MAX_LOG_SIZE, format, alp);
	va_end(alp);
	log_message[MAX_LOG_SIZE - 1] = 0;

	this->_xmit_logitem(tag, 0, 0, log_message, line);
	Error_Handler();



}

/*
 * Print a log message and continue
 */

void Logging::log(const char *tag, uint8_t level, uint32_t line, const char *format, ...) {

	va_list alp;
	va_start(alp, format);

	logItem li = {0};
	vsnprintf(li.log_message, MAX_LOG_SIZE, format, alp);
	va_end(alp);
	li.log_message[MAX_LOG_SIZE - 1] = 0;
	Utility.strncpy_term(li.tag, tag, MAX_TAG_SIZE);
	li.level = level;
	li.timestamp = osKernelGetTickCount();
	li.line = line;
	osStatus_t res = osMessageQueuePut(this->_queue_logging_handle, &li, 0U, 0U);
	if(res == osErrorResource) {
		this->_queue_overflow = true;
	}
	else if (res == osErrorParameter) {
		/* Program bug */
		Error_Handler();
	}
}

/*
 * Setup prior to running RTOS
 */

void Logging::setup(void) {

	/* Definitions for Logging Queue */

	const osMessageQueueAttr_t Queue_Logging_Attributes = {
	  .name = "Queue_Logging"
	};

	/* Create logging queue */

	this->_queue_logging_handle = osMessageQueueNew (LOG_QUEUE_DEPTH, sizeof(logItem), &Queue_Logging_Attributes);

	this->_queue_overflow = false;

}

/*
 * Initialize logging task
 */

void Logging::init(void) {
	/* Worker thread attributes */
	static const osThreadAttr_t worker_attr = {
			"LoggingWorkerThread",
			osThreadDetached,
			NULL,
			0,
			NULL,
			1024,
			osPriorityLow,
			0,
			0
	};

	/* Create worker task */
	if(osThreadNew(_worker, NULL, &worker_attr) == NULL) {
		LOG_PANIC(TAG, "Could not start worker thread");
	}
}

/*
 * Process log messages from the queue.
 */

void Logging::loop() {

	logItem li;

	osStatus_t res = osMessageQueueGet(this->_queue_logging_handle, &li, NULL, 0U);

	if (res == osOK) {
		this->_xmit_logitem(li.tag, li.level, li.timestamp, li.log_message, li.line);
	}
	else if (res == osErrorParameter) {
		/* Program Bug if this happens */
		Error_Handler();
	}



	if (this->_queue_overflow) { /* Test for log buffer overflow */
		this->_queue_overflow = false;
		this->_xmit_logitem(TAG, LOGGING_ERROR, osKernelGetTickCount(), "*** Log buffer overflow ***", __LINE__);

	}




}

} /* End Namespace LOGGING */

LOGGING::Logging Logger;


