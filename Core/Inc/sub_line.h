#pragma once
#include "top.h"

namespace Sub_Line {

const uint8_t MAX_DUAL_LINE_CARDS = 4;
const uint8_t LINE_CARD_I2C_ADDRESS = 0x30;
const uint8_t EVENT_MESSAGE_LENGTH = 2;

/* Register addresses for commands and status */
enum {REG_GET_EVENT=0, REG_GET_BUSY_STATUS=1, REG_SET_OR_ATTACHED=2, REG_SET_IN_CALL=3, REG_REQUEST_RINGING=4, REG_END_CALL=5, REG_POWER_CTRL=254};

/* Events received from card */
enum {EV_NONE=0, EV_READY=1, EV_REQUEST_OR=2, EV_DIALED_DIGIT=2, EV_HOOKFLASH=3, EV_BUSY=4, EV_RINGING=5, EV_ANSWERED=6, EV_HUNGUP=7, };




} /* End namespace Sub_Line */
