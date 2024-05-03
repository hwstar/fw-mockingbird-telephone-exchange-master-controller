#include "top.h"
#include "event.h"
#include "logging.h"
#include "drv_atten.h"
#include "drv_dtmf.h"
#include "card_comm.h"
#include "sub_line.h"
#include "trunk.h"


static const char *TAG = "event";


/*
 * Trick to call a C++ method from the RTOS
 */

Event::Event Event_handler;

static void _worker(void *args) {
	Event_handler.worker(args);
}

namespace Event {


/*
 * Initialization
 */

void Event::init(void) {


	/* Mutex attributes */
	static const osMutexAttr_t event_mutex_attr = {
		"EventMutex",
		osMutexRecursive | osMutexPrioInherit,
		NULL,
		0U
	};

	/* Worker thread attributes */
	static const osThreadAttr_t worker_attr = {
			"EventWorkerThread",
			osThreadDetached,
			NULL,
			0,
			NULL,
			1024,
			osPriorityHigh,
			0,
			0
	};


	/* Create mutex to protect event data between tasks */
	this->_lock = osMutexNew(&event_mutex_attr);
	if (this->_lock == NULL) {
		LOG_PANIC(TAG, "Could not create lock");
	}

	/* Create worker task */
	if(osThreadNew(_worker, NULL, &worker_attr) == NULL) {
		LOG_PANIC(TAG, "Could not start worker thread");
	}


}


/*
 * Line handler static function
 */

static void __line_handler(uint32_t event_type, uint32_t resource ) {
	Sub_line.event_handler(event_type, resource);
}

/*
 * Trunk handler static function
 */

static void __trunk_handler(uint32_t event_type, uint32_t resource ) {

	Trunks.event_handler(event_type, resource);

}


/*
 * Worker thread
 */

void Event::worker(void *args) {
	uint32_t card = 0;
	Card_Comm::Event_Handler handler = NULL;


	/*
	 * This loop handles setting up and taking down calls, polling the DTMF receivers, and looking for attention events
	 * It runs once every 10 milliseconds.
	 */


	for(;;) {
		osDelay(10);

		/* Poll DTMF receivers */
		Dtmf_receivers.poll();

		/* Poll ONE card on each 10ms pass */

		bool atten = Attention.get_state(card);

		if(atten) {
			if(card >= Sub_Line::MAX_DUAL_LINE_CARDS) {
				handler = __trunk_handler;
			}
			else {
				handler = __line_handler;
			}
			Card_comm.queue_get_event_request(card, handler);
		}


		/* Card to poll next time */
		card += 1;
		if(card >= Atten::MAX_NUM_CARDS) {
			card = 0;
		}

		Trunks.poll();
		Sub_line.poll();
	}

}


} /* End namespace Event */

