#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#include <stdio.h>
#include <string.h>

//#include "net/ipv6/uip-ds6.h"
//#include "net/ip/uip-udp-packet.h"
//#include "net/ip/uip.h"
#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/button-sensor.h"

/*
static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;
static struct runicast_conn runicast;
static struct runicast_callbacks data_runicast_call = 0;
*/

PROCESS(unicast_process, "unicast process");
AUTOSTART_PROCESSES(&unicast_process);

static void recv_unicast(struct unicast_conn *conn, const linkaddr_t *from){
	printf("unicast received from %d.%d\n",from->u8[0],from->u8[1]);
}
static void sent_unicast(struct unicast_conn *conn, int status, int num_tx){
	const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
	if(linkaddr_cmp(dest, &linkaddr_null)){
		return;
	}
	printf("unicast sent to %d.%d: status %d num_tx: %d\n",dest->u8[0],dest->u8[1],status,num_tx);
}

static const struct unicast_callbacks unicast_callbacks = {recv_unicast,sent_unicast};
static struct unicast_conn uconn;

PROCESS_THREAD(unicast_process, ev, data){
	PROCESS_BEGIN();
	SENSORS_ACTIVATE(button_sensor);
	while(1){
		PROCESS_WAIT_EVENT();
		if(ev == sensors_event){
			printf("button pressed\n");
			unicast_open(&uconn,146,&unicast_callbacks);
			linkaddr_t addr;
			packetbuf_copyfrom("coucou",6);
			addr.u8[0] = 1;
			addr.u8[1] = 0;
			if(!linkaddr_cmp(&addr,&linkaddr_node_addr)){
				unicast_send(&uconn,&addr);
			}
		}
	}
	PROCESS_END();
}


