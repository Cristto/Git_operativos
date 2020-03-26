#include "Clock.h"
#include "Processor.h"
#include "ComputerSystemBase.h"

int tics=0;


void Clock_Update() {

	tics++;
	// V2 Ej 2.d
	//ex-n
	if(tics==intervalBetweenInterrupts){
		Processor_RaiseInterrupt(CLOCKINT_BIT);
	}
	//end ex-n
    // ComputerSystem_DebugMessage(97,CLOCK,tics);
}


int Clock_GetTime() {

	return tics;
}
