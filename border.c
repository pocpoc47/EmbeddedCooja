#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#include <stdio.h>
#include <string.h>

#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/button-sensor.h"
#include "dev/serial-line.h"

#include "node.h"

static linkaddr_t my_parent;
static linkaddr_t children[MAX_CHILDREN];
static int neighbours[MAX_NEIGHBOURS];
static linkaddr_t route[MAX_NEIGHBOURS];
static linkaddr_t server_addr;

int num_children = 0;
int has_parent = 0;

PROCESS(unicast_process, "unicast process");
AUTOSTART_PROCESSES(&unicast_process);

static void print_array(linkaddr_t* array,int* arraySize){
	int i;
	for(i = 0;i < *arraySize; i++){
		printf("%d, ",array[i].u8[0]);
	}
	printf("\n");
}
static void print_children(){
	printf("My %d children: ",num_children);
	print_array(children,&num_children);
}
static void print_route(){
	printf("My routes: ");
	int i;
	for(i = 0;i < MAX_NEIGHBOURS;i++){
		printf("%d, ",route[i].u8[0]);
	}
	printf("\n");

}
static void print_neighbours(){
	printf("My neighbours: ");
	int i ;
	for(i = 0;i < MAX_NEIGHBOURS;i++){
		if(neighbours[i]!=0){
			printf("%d, ",i);
		}
	}
	printf("\n");
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
//is this addr my neighbour?
//if its score is > 0, returns TRUE
static int contains_nb(linkaddr_t* addr){
	return neighbours[addr->u8[0]];
}

static int insert_addr(linkaddr_t* addr, linkaddr_t* array,int* arraySize){
	if(contains_addr(addr,array,arraySize)){
		return 0;
	}
	array[(*arraySize)++] = *addr;
	//printf("added %d.%d to the list\n",addr->u8[0],addr->u8[1]);
	return 1;
}
static void add_neighbour(const linkaddr_t* addr){
	neighbours[addr->u8[0]] = 3;
}

//I received a discover by broadcast from disc
static void recv_discover(linkaddr_t* disc){
	packet pck;
	//printf("DISCOVER FROM %d\n",disc->u8[0]);
	//he is now my neihgbour
	add_neighbour(disc);
	//I respond with a HELLO packet, ORPHAN if I need a parent, CHILD otherwise
	pck.type = HELLO_CHILD;
	pck.dst = *disc;
	pck.src = linkaddr_node_addr;
	packetbuf_clear();
	packetbuf_copyfrom(&pck,sizeof(packet));
	unicast_send(&uconn,&pck.dst);
}

//callback function for a received broadcast packet
static void recv_broadcast(struct broadcast_conn *conn, const linkaddr_t *from){
	//printf("broadcast received from %d.%d\n",from->u8[0],from->u8[1]);
	packet* pck_rcv = (packet*) packetbuf_dataptr();
        
	if(pck_rcv->type == DISCOVER){
		recv_discover(&pck_rcv->src);
	}
}

//callback function for a sent broadcast packet
static void sent_broadcast(struct broadcast_conn *conn, int status, int num_tx){
	//printf("Broadcast sent: status %d num_tx: %d\n",status,num_tx);
}



//remove nodes that are children but not neighbours
static void remove_dead_children(){
	//printf("removing dead children...\n");
	linkaddr_t temp_children[MAX_CHILDREN];
	int i;
	int new_i = 0;
	for(i=0;i<num_children;i++){
		//if the children is in the neighbours, put it again into children
		//printf("is %d in my neighbours? \n",children[i].u8[0]);
		if(contains_nb(&children[i])){
			temp_children[new_i++] = children[i];
			//printf("yes\n");
		}
		else{
			printf("%d is no longer my neighbour and has been removed from my children\n",children[i].u8[0]);
		}
	}
        //repopulate the children array
	for(i=0;i<new_i;i++){
		children[i]=temp_children[i];
	}
	num_children = new_i;
}
//ask a node to adopt him
static void adopt(linkaddr_t* child){
	packet pck;
	pck.type = CHILD_HELLO;
	pck.src = linkaddr_node_addr;
	pck.dst = *child;
	//printf("trying to adopt %d\n",child->u8[0]);
	packetbuf_clear();
	packetbuf_copyfrom(&pck,sizeof(packet));
	unicast_send(&uconn,child);
}

//I received a HELLO packet from a neighbour as response to my DISCOVER
static void recv_hello(packet* pck){
	//if I have room for a child, and he is orphan, adopt him
	//add him to the neighbours list
	add_neighbour(&pck->src);
	//printf("received hello from %d\n",pck->src.u8[0]);
	if(pck->type==HELLO_ORPHAN && num_children < MAX_CHILDREN && !linkaddr_cmp(&my_parent,&pck->src)){
		adopt(&pck->src);
	}

	
	//printf("%d.%d is my neighbour\n",pck->src.u8[0],pck->src.u8[1]);
}
//remove one point from every neighbour
//it one reaches, it will considered dead
static void trim_neighbours(){
	int i;
	for(i = 0;i<MAX_NEIGHBOURS;i++){
		if(neighbours[i]>0){
			neighbours[i]--;
		}
	}
}
//Sending a broadcast to discover a node's neighbours
static void discover(){
	//print_neighbours();
	//print_children();
	//print_route();
	trim_neighbours();
	remove_dead_children();


	packet pck;
	pck.type = DISCOVER;
	pck.src = linkaddr_node_addr;
	pck.dst = linkaddr_null;
	packetbuf_clear();
	packetbuf_copyfrom(&pck,sizeof(pck));
	broadcast_send(&bconn);
}
//add route to node dst, through child
static void add_route(const linkaddr_t* dst, const linkaddr_t* child){
	route[dst->u8[0]] = *child;
	//printf("Route %d, send to %d\n", dst->u8[0], child->u8[0]);
}

//received an adoption confirmation (PARENT_ACK) from a child
//insert him in my children list
static void confirm_adoption(linkaddr_t* child){
	//printf("confirming addoption of %d\n",child->u8[0]);
	add_route(child, child);
	insert_addr(child,children,&num_children);
	printf("%d is now my child\n",child->u8[0]);
}
//received data from a sensor,
//printf sends it through the serial interface
//to the server
static void recv_data(packet* rcv_pck){
	printf("received data from %d\n", rcv_pck->src.u8[0]);
       
        printf("%d/%d\n",rcv_pck->src.u8[0],rcv_pck->data);
}

//send the command command to the node to
static void send_data(linkaddr_t* to,int command){
	packet pck;
	pck.src = linkaddr_node_addr;
	printf("Sending command %d to %d\n",command,to->u8[0]);
	pck.dst = *to;
	pck.type= command;
	packetbuf_copyfrom(&pck,sizeof(pck));
	unicast_send(&uconn, &route[to->u8[0]]);
	
}
//received a command from the server through serial
//parse the command and sends it to the node
static void recv_command(char* command){
        int id;
        int com;
        int len = strlen(command);
        if(len==4){
            id = 10*(command[0]-'0') + command[1]-'0';
            com = command[3]-'0';
        }
        else if(len==3){
            id = command[0]-'0';
            com = command[2]-'0';
        }
        else{
            printf("wrong format");
        }

        linkaddr_t node;
        node.u8[0] = id;
        node.u8[1] = 0;
        send_data(&node, com);
}
//callback funtion for a received unicast packet
static void recv_unicast(struct unicast_conn *conn, const linkaddr_t *from){
	packet* pck = (packet*)packetbuf_dataptr();
	//add the src to its neighbours
        add_neighbour(from);
        //add a route
	add_route(&pck->src, from);

        //is the packet for me?
        //the border should only receive unicast destined to himself
	if(linkaddr_cmp(&pck->dst,&linkaddr_node_addr)){
		if(pck->type==SENSOR_DATA){
			recv_data(pck);
		}
		else if(pck->type==HELLO_ORPHAN || pck->type==HELLO_CHILD){
			recv_hello(pck);
		}
	}
	else{
            printf("not for me\n")
	}
}
static void sent_unicast(struct unicast_conn *conn, int status, int num_tx){
	//printf("unicast sent to %d.%d: status %d num_tx: %d\n",dest->u8[0],dest->u8[1],status,num_tx);
}


static const struct unicast_callbacks unicast_callbacks = {recv_unicast,sent_unicast};
static const struct broadcast_callbacks broadcast_callbacks = {recv_broadcast,sent_broadcast};

PROCESS_THREAD(unicast_process, ev, data){
	PROCESS_BEGIN();
	server_addr.u8[0] = 1;
	server_addr.u8[1] = 0;
	SENSORS_ACTIVATE(button_sensor);
	unicast_open(&uconn,146,&unicast_callbacks);
	broadcast_open(&bconn,129,&broadcast_callbacks);
	static struct etimer et;
	etimer_set(&et, CLOCK_SECOND*linkaddr_node_addr.u8[0]*2);
	while(1){
		PROCESS_WAIT_EVENT();
		if(ev == sensors_event){

			printf("button pressed\n");
		}
		else if(etimer_expired(&et)){
			discover();
			etimer_set(&et, CLOCK_SECOND*DISCOVER_INTERVAL);
		}
                //serial event from the server
		else if(ev == serial_line_event_message){
                        recv_command((char*)data);
		}
	}
	PROCESS_END();
}


