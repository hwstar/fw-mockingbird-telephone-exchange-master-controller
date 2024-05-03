#pragma once
#include "top.h"
#include "xps_logical.h"

namespace Connector {

typedef struct Conn_Info {
	uint8_t state;
	bool junctor_seized;
	int16_t tone_plant_descriptor;
	int16_t mf_receiver_descriptor;
	int16_t dtmf_receiver_descriptor;
	XPS_Logical::Junctor_Info jinfo;


}Conn_Info;

}
