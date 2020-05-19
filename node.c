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

static linkaddr_t parent;
static linkaddr_t children[MAX_CHILDREN];
static linkaddr_t neighbours[10];
static linkaddr_t orphan_neighbours[10];
static linkaddr_t server_addr;

int num_children = 0;
int num_neighbours = 0;
int num_orphan_neighbours = 0;
int has_parent = 0;

PROCESS(unicast_process, "unicast process");
AUTOSTART_PROCESSES(&unicast_process);
static void recv_broadcast(struct broadcast_conn *conn, const linkaddr_t *from){
	//printf("broadcast received from %d.%d\n",from->u8[0],from->u8[1]);
	packet* pck_rcv = (packet*) packetbuf_dataptr();
	if(pck_rcv->type == DISCOVER){
		packet pck;
		if(has_parent){
			pck.type = HELLO_CHILD;
		}
		else{
			pck.type = HELLO_ORPHAN;
		}
		pck.dst = pck_rcv->src;
		pck.src = linkaddr_node_addr;
		pck.message = "hello je t'entends\n";
		packetbuf_copyfrom(&pck,sizeof(packet));
		unicast_send(&uconn,&pck.dst);
	}

	
}
static void sent_broadcast(struct broadcast_conn *conn, int status, int num_tx){
	printf("Broadcast sent: status %d num_tx: %d\n",status,num_tx);
}

static int contains_addr(linkaddr_t* addr, linkaddr_t* array,int* arraySize){
	int i;
	for(i=0;i < *arraySize;i++){
		if(linkaddr_cmp(addr,array+i)){
			return 1;
		}
	}
	return 0;
}

static int insert_addr(linkaddr_t* addr, linkaddr_t* array,int* arraySize){
	if(contains_addr(addr,array,arraySize)){
		return 0;
	}
	/*
	printf("pointer: %p\n",array);
	printf("size = %d\n",*arraySize);
	printf("0: %d\n",array[0].u8[0]);
	printf("1: %d\n",array[1].u8[0]);
	printf("2: %d\n",array[2].u8[0]);
	*/
	array[(*arraySize)++] = *addr;
	//printf("added %d.%d to the list\n",addr->u8[0],addr->u8[1]);
	return 1;
}
static void remove_dead_children(){
	linkaddr_t temp_children[MAX_CHILDREN];
	int i;
	int new_i = 0;
	for(i=0;i<num_children;i++){
		//if the children is in the neighbours, put it again into children
		if(!contains_addr(&children[i],neighbours,&num_neighbours)){
			temp_children[new_i++] = children[i];
		}
		else{
			printf("%d.%d is no longer my neighbour and has been removed from my children\n",children[i].u8[0],children[i].u8[1]);
		}
	}
	for(i=0;i<new_i;i++){
		children[i]=temp_children[i];
	}
	num_children = new_i;
}
static void adopt(linkaddr_t* child){
	packet pck;
	pck.type = CHILD_HELLO;
	pck.src = linkaddr_node_addr;
	pck.dst = *child;
	pck.message = "I want to adopt you\n";
	packetbuf_copyfrom(&pck,sizeof(packet));
	unicast_send(&uconn,child);
}
static void adopt_children(){
	int i;
	for(i = 0;i < MAX_CHILDREN-num_children || i < num_orphan_neighbours;i++){
		adopt(&orphan_neighbours[i]);
	}
}
static void print_array(linkaddr_t* array,int* arraySize){
	int i;
	for(i = 0;i < *arraySize; i++){
		printf("%d.%d, ",array[i].u8[0],array[i].u8[1]);
	}
	printf("\n");
}
static void print_children(){
	printf("My children: ");
	print_array(children,&num_children);
}
static void print_neighbours(){
	printf("My neighbours: ");
	print_array(neighbours,&num_neighbours);
}
			

static void recv_hello(packet* pck){
	insert_addr(&pck->src,neighbours,&num_neighbours);
	if(pck->type==HELLO_ORPHAN){
		insert_addr(&pck->src,orphan_neighbours,&num_orphan_neighbours);
	}
	
	printf("%d.%d is my neighbour\n",pck->src.u8[0],pck->src.u8[1]);
}
static void get_adopted(linkaddr_t* parent){

	if(linkaddr_cmp(&linkaddr_node_addr,&server_addr)){
		printf("root doesn't need a parent duh\n");
	}
	else if(!has_parent){
		printf("%d.%d is now my parent\n",parent->u8[0],parent->u8[1]);
		packet pck_ack;
		pck_ack.type = PARENT_ACK;
		pck_ack.dst = *parent;
		pck_ack.src = linkaddr_node_addr;
		pck_ack.message = "You adopted me\n";
		has_parent = 1;
		packetbuf_copyfrom(&pck_ack,sizeof(packet));
		unicast_send(&uconn,parent);
	}
	else{
		printf("already have a parent\n");
	}
}
static void discover(){
	num_neighbours = 0;
	num_orphan_neighbours = 0;
	//print_children();
	//adopt_children();
	//remove_dead_children();
	//quand appeler ces fonctions? idealement apres avoir recu tous les HELLO des voisins, mais comment le savoir?
	packet pck;
	pck.type = DISCOVER;
	pck.src = linkaddr_node_addr;
	pck.dst = linkaddr_null;
	pck.message = "Est-que qqn m'entend?\n";
	packetbuf_copyfrom(&pck,sizeof(pck));
	broadcast_send(&bconn);
}

static void recv_unicast(struct unicast_conn *conn, const linkaddr_t *from){
	packet* pck = (packet*)packetbuf_dataptr();
	if(linkaddr_cmp(&pck->dst,&linkaddr_node_addr)){
		if(pck->type==SENSOR_DATA){
			printf("received message from %d.%d\n",from->u8[0],from->u8[1]);
			printf(pck->message);
		}
		else if(pck->type==PARENT_ACK){
			children[num_children++] = pck->src;
				printf("%d.%d is now my child\n",pck->src.u8[0],pck->src.u8[1]);
				if(num_children < MAX_CHILDREN){
			}
		}
		else if(pck->type==HELLO_ORPHAN || pck->type==HELLO_CHILD){
			recv_hello(pck);
		}
		if(pck->type == CHILD_HELLO){
			get_adopted(&pck->src);
		}
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
/*
static void child_discovery(){
	DISCOVER;
	are my children all alive?
	remove dead children;
	max children?
	adopt orphan neighbour 

*/


static const struct unicast_callbacks unicast_callbacks = {recv_unicast,sent_unicast};
static const struct broadcast_callbacks broadcast_callbacks = {recv_broadcast,sent_broadcast};

PROCESS_THREAD(unicast_process, ev, data){
	PROCESS_BEGIN();
	server_addr.u8[0] = 1;
	server_addr.u8[1] = 0;
	SENSORS_ACTIVATE(button_sensor);
	unicast_open(&uconn,146,&unicast_callbacks);
	broadcast_open(&bconn,129,&broadcast_callbacks);
	while(1){
		PROCESS_WAIT_EVENT();
		if(ev == sensors_event){

			printf("button pressed\n");
			/*
			if(children[0].u8[0]!= 0){
				packet pck;
				pck.type = SENSOR_DATA;
				pck.src = linkaddr_node_addr;
				pck.dst = server_addr;
				pck.message = "message de moi\n";
				packetbuf_copyfrom(&pck,sizeof(pck));
				unicast_send(&uconn,&parent);
			}	
			*/
			print_neighbours();
			discover();
		}
		else{
			printf("RIEN\n");
		}
	}
	PROCESS_END();
}


