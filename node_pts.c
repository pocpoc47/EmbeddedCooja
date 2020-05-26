#include <stdio.h>
#include <string.h>

#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/button-sensor.h"
#include "dev/serial-line.h"


PROCESS(serial_process, "serial process");
AUTOSTART_PROCESSES(&serial_process);

PROCESS_THREAD(serial_process, ev, data){
	PROCESS_BEGIN();
	SENSORS_ACTIVATE(button_sensor);
	while(1){
		PROCESS_WAIT_EVENT();
		if(ev == sensors_event){
			printf("hello pts\n");
		}
		if(ev == serial_line_event_message){
			printf("received %s\n",(char*) data);
		}
	}
	PROCESS_END();
}


