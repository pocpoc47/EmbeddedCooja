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

#include "node.h"

/*
static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;
static struct runicast_conn runicast;
static struct runicast_callbacks data_runicast_call = 0;
*/

linkaddr_t parent;
linkaddr_t children[3];
linkaddr_t emptyaddr;

PROCESS(unicast_process, "unicast process");
AUTOSTART_PROCESSES(&unicast_process);
static void recv_broadcast(struct broadcast_conn *conn, const linkaddr_t *from){
	//printf("broadcast received from %d.%d\n",from->u8[0],from->u8[1]);
	packet* pck = (packet*) packetbuf_dataptr();

	if(linkaddr_node_addr.u8[0] ==1){
		printf("root doesn't need a parent duh\n");
	}
	else if(parent.u8[0] == 0 && pck->type == 1){
		parent = *from;
		printf("updated parent %d.%d\n",parent.u8[0],parent.u8[1]);
		packet pck_ack;
		pck_ack.type = 2;
		pck_ack.dst = parent;
		pck_ack.src = linkaddr_node_addr;
		pck_ack.message = "tu es mon pere\n";
		packetbuf_copyfrom(&pck_ack,sizeof(packet));
		unicast_send(&uconn,&parent);
	}
	else{
		printf("already have a parent\n");
	}
}
static void sent_broadcast(struct broadcast_conn *conn, int status, int num_tx){
	printf("Broadcast sent: status %d num_tx: %d\n",status,num_tx);
}

static void recv_unicast(struct unicast_conn *conn, const linkaddr_t *from){
	packet* pck = (packet*)packetbuf_dataptr();
	if(pck->type==3 && pck->dst.u8[0] == linkaddr_node_addr.u8[0]){
		printf("received message from %d.%d\n",from->u8[0],from->u8[1]);
	}
	else if(pck->type==2){
		children[0] = pck->src;
		printf("%d.%d is now my child\n",pck->src.u8[0],pck->src.u8[1]);
	}
	else{
		packetbuf_copyfrom(pck,sizeof(*pck));
		unicast_send(&uconn,&parent);
		printf("from %d.%d relay to parent %d.%d\n",from->u8[0],from->u8[1],parent.u8[0],parent.u8[1]);
	}
}
static void sent_unicast(struct unicast_conn *conn, int status, int num_tx){
	const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
	if(linkaddr_cmp(dest, &linkaddr_null)){
		return;
	}
	//printf("unicast sent to %d.%d: status %d num_tx: %d\n",dest->u8[0],dest->u8[1],status,num_tx);
}

static const struct unicast_callbacks unicast_callbacks = {recv_unicast,sent_unicast};
static const struct broadcast_callbacks broadcast_callbacks = {recv_broadcast,sent_broadcast};

PROCESS_THREAD(unicast_process, ev, data){
	PROCESS_BEGIN();
	emptyaddr.u8[0] = 0;
	emptyaddr.u8[1] = 0;
	children[0]=emptyaddr;
	children[1]=emptyaddr;
	children[2]=emptyaddr;
	SENSORS_ACTIVATE(button_sensor);
	unicast_open(&uconn,146,&unicast_callbacks);
	broadcast_open(&bconn,129,&broadcast_callbacks);
	while(1){
		PROCESS_WAIT_EVENT();
		if(ev == sensors_event){

			printf("button pressed\n");
			if(children[0].u8[0]!= 0){
				packetbuf_copyfrom("Message to server\n",18);
				unicast_send(&uconn,&parent);
			}	
			else{
				packet pck;
				pck.type = 1;
				pck.src = linkaddr_node_addr;
				pck.src = linkaddr_null;
				pck.message = "je suis ton pere\n";
				packetbuf_copyfrom(&pck,sizeof(pck));
				broadcast_send(&bconn);
			}
		}
		else{
			printf("RIEN\n");
		}
	}
	PROCESS_END();
}


