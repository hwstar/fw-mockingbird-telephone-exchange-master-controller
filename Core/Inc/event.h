#pragma once
#include "top.h"

namespace Event {

class Event {
public:
	void init(void);
	void worker(void *args);

protected:
	osMutexId_t _lock;

};


} /* End Namespace Event */

extern Event::Event Event_handler;

