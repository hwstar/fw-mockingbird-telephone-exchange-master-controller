#pragma once
#include "top.h"

#define POST_ERROR(err_code) Err_handler.post(err_code, TAG, __LINE__)
#define POST_ERROR_ADDL_INFO(error_code, info) Err_handler.post(err_code, TAG, __LINE__, info)

namespace Err_Handler {

/* Error Codes */
enum {
	EH_NPFA=0, /* Null pointer function argument passed in */
	EH_INVP=1, /* Invalid function parameter passed in */
	EH_BRV=2, /* Bad return value */
	EH_ETNT=3, /* Equipment type must be ET_TRUNK */
	EH_IRS=4, /* Invalid route state */
	EH_IRT=5, /* Invalid resource type */
	EH_IET=6, /* Invalid equipment type */
	EH_UET=7, /* Unsupported equipment type */
	EH_ITN=8, /* Invalid trunk number */
	EH_LCE=9, /* Could not create lock */
	EH_INVR=10, /* Invalid result */
	EH_TCE=11, /* Could not create timer */
	EH_UHC=12, /* Case not handled*/
	EH_IPLN=13, /* Invalid physical line number */
	EH_IVCN=14, /* Invalid card number */
	EH_LAF=15, /* Lock acquisition failed */
	EH_IVR=16, /* Invalid receiver */
	EH_IVD=17, /* Invalid descriptor */
	EH_ICSN=18, /* Invalid CS number */
	EH_IVXY=19, /* Invalid X or Y inputs */
	EH_TSF=20, /* Thread start failed */
	EH_NOSD=21, /* SD Card not present, insert SD Card and reboot */
	EH_MQGE=22, /* Message queue get error */
	EH_MQCF=23, /* Message queue creation failed */
	EH_NOTS=24, /* Timer start failed */
	EH_FRAA=25, /* Attempt made to free receiver already available */
	EH_AOOM=26, /* Allocator out of memory */
	EH_MCD=27, /* Memory corruption detected */
	EH_ISAI=28, /* Invalid SAI channel number */
	EH_ICPT=29, /* Invalid call progress type */
	EH_LPME=30, /* Logical pin mapping error */
	EH_IJN=31, /* Invalid junctor number */
	EH_CNRC=32, /* Connection resource conflict */
	EH_NOCF=33, /* Could not open config file */
	EH_CFER=34, /* Configuration file error */
	EH_FSER=35, /* File system error */
	EH_CFSE=36, /* Configuration file syntax error */
	EH_NMA=37, /* No memory Available */
	EH_NSFL=38, /* No Such file */

	EH_NUM_ERROR_CODES
};
/* Error Actions */
enum {ACTION_PANIC=0, ACTION_RECOVERABLE_ERROR=2, ACTION_WARNING=3, ACTION_NOTE=4};

typedef struct Error_Table_Entry {
	uint8_t error_action;
	const char *error_message;
} Error_Table_Entry;

class Err_Handler {
protected:
public:
	void post(uint16_t error_code, const char *tag, uint32_t line, const char *addl_info = NULL);

};


} /* End namespace Err_Handler */

extern Err_Handler::Err_Handler Err_handler;
