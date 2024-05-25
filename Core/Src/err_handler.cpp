#include "top.h"
#include "logging.h"
#include "err_handler.h"
#include "util.h"


namespace Err_Handler {
const char *TAG = "errhand";

static const Error_Table_Entry Error_table[] = {
		{ACTION_PANIC, "Null pointer function argument passed in"}, /* 0 */
		{ACTION_PANIC, "Invalid function parameter passed in"},
		{ACTION_PANIC, "Bad return value"},
		{ACTION_PANIC, "Equipment type must be ET_TRUNK"},
		{ACTION_PANIC, "Invalid route state" },
		{ACTION_PANIC, "Invalid resource type"},
		{ACTION_PANIC, "Invalid equipment type"},
		{ACTION_PANIC, "Unsupported equipment type"},
		{ACTION_PANIC, "Invalid trunk number"},
		{ACTION_PANIC, "Could not create lock"},
		{ACTION_PANIC, "Invalid result"}, /* 10 */
		{ACTION_PANIC, "Could not create timer"},
		{ACTION_PANIC, "Case not handled"},
		{ACTION_PANIC, "Invalid physical line number"},
		{ACTION_PANIC, "Invalid card number"},
		{ACTION_PANIC, "Lock acquisition failed"},
		{ACTION_PANIC, "Invalid receiver"},
		{ACTION_PANIC, "Invalid descriptor"},
		{ACTION_PANIC, "Invalid CS number"},
		{ACTION_PANIC, "Invalid X or Y inputs"},
		{ACTION_PANIC, "Thread start failed"}, /* 20 */
		{ACTION_PANIC, "SD Card not present, insert SD Card and reboot"},
		{ACTION_PANIC, "Message queue get error"},
		{ACTION_PANIC, "Message queue creation failed"},
		{ACTION_PANIC, "Timer start failed"},
		{ACTION_PANIC, "Attempt made to free receiver already available"},
		{ACTION_PANIC, "Allocator out of memory"},
		{ACTION_PANIC, "Memory corruption detected"},
		{ACTION_PANIC, "Invalid SAI channel number"},
		{ACTION_PANIC, "Invalid call progress type"},
		{ACTION_PANIC, "Logical pin mapping error"}, /* 30 */
		{ACTION_PANIC, "Invalid junctor number"},
		{ACTION_PANIC, "Connection resource conflict"},
		{ACTION_PANIC, "Could not open config file"},
		{ACTION_PANIC, "Configuration file error"},
		{ACTION_PANIC, "File system error"},
		{ACTION_PANIC, "Configuration file syntax error"},
		{ACTION_PANIC, "No memory Available"},
		{ACTION_PANIC, "No Such file"},










};

void Err_Handler::post(uint16_t error_code, const char *tag, uint32_t line, const char *addl_info) {
	osDelay(300); /* Wait for queued log messages to get sent */
	if(!tag) {
		LOG_PANIC("TAG", Error_table[EH_NPFA].error_message);
	}
	if(error_code >= EH_NUM_ERROR_CODES) {
		LOG_PANIC(TAG, "Unknown error code passed in");

	}
	if(Error_table[error_code].error_action == ACTION_PANIC) {
		if(!addl_info) {
			Logger.panic(tag, line, "E%04u:%s", error_code, Error_table[error_code].error_message);
		}
		else {
			Logger.panic(tag, line, "E%04u:%s: %s", error_code, Error_table[error_code].error_message, addl_info);
		}

	}
	else {

	}


}

} /* End namespace Error_Handler */

Err_Handler::Err_Handler Err_handler;
