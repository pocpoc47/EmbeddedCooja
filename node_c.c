#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#include <stdio.h>
#include <string.h>

#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"

#include "node.h"
#include "least_square.h"


static linkaddr_t my_parent;
static linkaddr_t children[MAX_CHILDREN];
static int neighbours[MAX_NEIGHBOURS];
static linkaddr_t route[MAX_NEIGHBOURS];
static linkaddr_t server_addr;

//list of the nodes it will handle
static linkaddr_t computated[MAX_COMP];
static int num_comp = 0;
//metadata for each node
static int comp_data_index[MAX_COMP][2];
//keep-alive score of each nod
static int comp_score[MAX_COMP];
//actual data for each node
static int comp_data[MAX_COMP][MAX_DATA];


static int valve_to_close;


int num_children = 0;
int has_parent = 0;

PROCESS(unicast_process, "unicast process");
PROCESS(close_valve, "close valva");
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
static void print_parent(){
	printf("My parent: %d.%d\n",my_parent.u8[0],my_parent.u8[1]);
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
	//he is now my neihgbour
	//printf("DISCOVER FROM %d\n",disc->u8[0]);
	add_neighbour(disc);
	//I respond with a HELLO packet, ORPHAN if I need a parent, CHILD otherwise
	if(has_parent){
		pck.type = HELLO_CHILD;
	}
	else{
		pck.type = HELLO_ORPHAN;
	}
	pck.dst = *disc;
	pck.src = linkaddr_node_addr;
	packetbuf_clear();
	packetbuf_copyfrom(&pck,sizeof(packet));
	unicast_send(&uconn,&pck.dst);
}

static void recv_broadcast(struct broadcast_conn *conn, const linkaddr_t *from){
	//printf("broadcast received from %d.%d\n",from->u8[0],from->u8[1]);
	packet* pck_rcv = (packet*) packetbuf_dataptr();
	if(pck_rcv->type == DISCOVER){
		recv_discover(&pck_rcv->src);
	}
}

static void sent_broadcast(struct broadcast_conn *conn, int status, int num_tx){
	//printf("Broadcast sent: status %d num_tx: %d\n",status,num_tx);
}
//send a message to a child to notify that he is no longer its child
static void abandon(linkaddr_t* child){
	packet pck;
	pck.src = linkaddr_node_addr;
	pck.dst = *child;
	pck.type = ABANDON_CHILD;
	packetbuf_clear();
	packetbuf_copyfrom(&pck,sizeof(packet));
	printf("abondoning %d\n", child->u8[0]);
	unicast_send(&uconn,&pck.dst);
}

//parent is dead, removing as parent
static void get_abandoned(){
	int i;
	for(i=0;i<num_children;i++){
		if(!linkaddr_cmp(&linkaddr_null,&children[i])){
                    //abandon all children so that the tree can rebuild itself correclty
			abandon(&children[i]);
			children[i] = linkaddr_null;
		}
	}
	num_children = 0;
	has_parent = 0;
	leds_off(LEDS_GREEN);
	my_parent = linkaddr_null;
	printf("my parent is dead, I am now orphan\n");
}

//remove parent if he is no longer a neighbour
static void remove_dead_parent(){
	
	if(!contains_nb(&my_parent)){
		//parent is dead :(
		get_abandoned();
	}
}

//remove nodes that are children but not neighbours
static void remove_dead_children(){
	//printf("removing dead children...\n");
	linkaddr_t temp_children[MAX_CHILDREN];
	int i;
	int new_i = 0;
	for(i=0;i<num_children;i++){
		//if the children is in the neighbours, put it again into children
		//printf("is %d in my neighbours? pls answer me\n",children[i].u8[0]);
		if(contains_nb(&children[i])){
			temp_children[new_i++] = children[i];
			//printf("yes\n");
		}
		else{
			printf("%d is no longer my neighbour and has been removed from my children\n",children[i].u8[0]);
		}
	}
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
	//I DONT ADOPT IF I DONT HAVE A PARENT
	//add him to the neighbours list
	add_neighbour(&pck->src);
	//printf("received hello from %d\n",pck->src.u8[0]);
	if(has_parent && pck->type==HELLO_ORPHAN && num_children < MAX_CHILDREN && !linkaddr_cmp(&my_parent,&pck->src)){
		adopt(&pck->src);
	}

	
	//printf("%d.%d is my neighbour\n",pck->src.u8[0],pck->src.u8[1]);
}
//received an adoption request, answer PARENT_ACK if im an orphan, and saves parent as my actual parent
static void get_adopted(linkaddr_t* parent){

	if(!has_parent){
		printf("%d is now my parent\n",parent->u8[0]);
		packet pck_ack;
		pck_ack.type = PARENT_ACK;
		pck_ack.dst = *parent;
		pck_ack.src = linkaddr_node_addr;
		has_parent = 1;
		leds_on(LEDS_GREEN);
		my_parent = *parent;
		
		packetbuf_clear();
		packetbuf_copyfrom(&pck_ack,sizeof(packet));
		unicast_send(&uconn,parent);
	}
	else{
		printf("already have a parent\n");
	}
}
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
	print_parent();
	//print_route();
	trim_neighbours();
	remove_dead_children();
	if(has_parent){
		remove_dead_parent();
	}


	packet pck;
	pck.type = DISCOVER;
	pck.src = linkaddr_node_addr;
	pck.dst = linkaddr_null;
	packetbuf_clear();
	packetbuf_copyfrom(&pck,sizeof(pck));
	broadcast_send(&bconn);
}
static void add_route(linkaddr_t* dst, const linkaddr_t* child){
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
static void send_data(linkaddr_t* to, int command){
	packet pck;
	pck.src = linkaddr_node_addr;
	printf("Sending command %d to %d\n",command,to->u8[0]);
	pck.dst = *to;
	pck.type= command;
	packetbuf_copyfrom(&pck,sizeof(pck));
	unicast_send(&uconn, &route[to->u8[0]]);
}
//relay packet from server to the correct child, using the routing table
static void relay_child(packet* pck){
	//printf("from server to %d, relay to child %d\n",pck->dst.u8[0],route[pck->dst.u8[0]].u8[0]);
	packetbuf_clear();
	packetbuf_copyfrom(pck,sizeof(*pck));
	unicast_send(&uconn,&route[pck->dst.u8[0]]);
}
//relay packet at destination of the server to the parent
static void relay_parent(packet* pck,const linkaddr_t* from){
	packetbuf_clear();
	packetbuf_copyfrom(pck,sizeof(*pck));
	unicast_send(&uconn,&my_parent);
	//printf("from %d relay to parent %d\n",pck->src.u8[0],my_parent.u8[0]);
}

//remove the node at index index
static void free_comp(int index){
	printf("removing %d\n", computated[index].u8[0]);
	computated[index] = linkaddr_null;
	comp_data_index[index][0] = 0;
	comp_data_index[index][1] = 0;
	num_comp--;
}
//remove keep-alive points to all nodes, and remove if 0
static void remove_points(){
	int i;
	for(i = 0; i < MAX_COMP; i++){
		
		if(!linkaddr_cmp(&computated[i], &linkaddr_null)){
			comp_score[i]--;
			printf("%d has %d points left\n",computated[i].u8[0], comp_score[i]);
			if(comp_score[i] == 0){
				free_comp(i);
			}
		}
	}
}
static void print_data(int* tab, int id, int size){
	printf("data for %d: ", id);
	int i;
        for(i = 0; i < size;i++){
		printf("%d, ", tab[i]);
	}
	printf("\n");
}
//read the data for the node and put it in the right order in a new array
static void compute_data(int index){
	int num = comp_data_index[index][0];
	int next = comp_data_index[index][1];
	int i;
	int start;
        int tab[num];
	printf("data for %d: ", computated[index].u8[0]);
	if(comp_data_index[index][0]>=MAX_DATA){
            start = comp_data_index[index][1];
	}else{
            start = 0;
	}
	for(i = 0; i < num;i++){
            tab[i] = comp_data[index][(start+i)%MAX_DATA];
	}
	print_data(tab, computated[index].u8[0], num);
        //if there is less than 2, does not compute
        if(comp_data_index[index][0]<2){
            return;
        }
        //calculating the least Square function
        int prev = least_square(comp_data[index], comp_data_index[index][0]);
        printf("prevision = %d\n", prev);
        //if the orevision is > than THRESHOD, open the valve for 10min
        if(prev > THRESHOLD){
            valve_to_close = computated[index].u8[0];
            send_data(&computated[index], SENSOR_OPEN);
            //start a process to countdown the 10 minutes
            process_start(&close_valve, NULL);   
            //sending the address of the node to the process
            process_post_synch(&close_valve, PROCESS_EVENT_CONTINUE, &valve_to_close);
        }

}
//add a node to the list of node to compute
static void add_comp(linkaddr_t* addr){
	int i;
	for(i = 0; i < 5; i++){
		if(linkaddr_cmp(&linkaddr_null, &computated[i]))
			break;
	}
	if(i==5)printf("Error, no room for new comp\n");
	//printf("adding %d in computated at i = %d\n",addr->u8[0],i);
	computated[i] = *addr;
	comp_data_index[i][0] = 0;
	comp_data_index[i][1] = 0;
	num_comp++;
}
//add data to a node's array
static void add_data(linkaddr_t* addr, int data){
	int i;
	for(i = 0; i < MAX_COMP; i++){
		if(linkaddr_cmp(addr, &computated[i]))
			break;
	}
	if(i==MAX_COMP)printf("Error, addr not in comp\n");
	comp_score[i] = COMP_SCORE;

	int num = comp_data_index[i][0];
	int next = comp_data_index[i][1];
        //num is the number of values
        //next is the index where the next value should be written

	printf("num: %d, next: %d, trying to add %d\n", num, next, data);
	comp_data[i][next++] = data;
	next = next%MAX_DATA;
	if(num < MAX_DATA)num++;
	comp_data_index[i][0] = num;
	comp_data_index[i][1] = next;

	//printf("num: %d, next: %d, added  %d\n", num, next, data);

	compute_data(i);
}
//received data from a sensor

static int recv_data(packet* pck){
	linkaddr_t addr = pck->src;
	//printf("received from %d, data=%d\n",pck->src.u8[0],pck->data);
        //add the data if the node is known
	if(contains_addr(&addr, computated, &num_comp)){
		add_data(&addr, pck->data);
		return 1;
	}
        //add the node and data if possible
	else if(num_comp < MAX_COMP){
		add_comp(&addr);
		add_data(&addr, pck->data);
		return 1;
	}
        //return 0 if max nodes reached
	else{
		return 0;
	}
}
static void recv_unicast(struct unicast_conn *conn, const linkaddr_t *from){
	packet* pck = (packet*)packetbuf_dataptr();
	add_neighbour(from);
	//is the packet for me?
	if(linkaddr_cmp(&pck->dst,&linkaddr_node_addr)){
		//printf("RECEIVED PACKET TYPE%d\n", pck->type);
		if(pck->type==PARENT_ACK){
			confirm_adoption(&pck->src);
		}
		else if(pck->type==HELLO_ORPHAN || pck->type==HELLO_CHILD){
			recv_hello(pck);
		}
		else if(pck->type == CHILD_HELLO){
			get_adopted(&pck->src);
		}
		else if(pck->type == ABANDON_CHILD){
			get_abandoned();
		}
	}
	//is the packet for the server?
	else if(linkaddr_cmp(&pck->dst,&server_addr)){
		add_route(&pck->src, from);
		//printf("packet for server\n");
		if(pck->type==SENSOR_DATA){
			//recv_data returns 1 if the data has been added, 0 if no room for new comp
			if(!recv_data(pck)){
				relay_parent(pck, from);
			}
		}
		else{
			printf("unknown type\n");
		}
	}
	else{

		relay_child(pck);
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
	server_addr.u8[0] = 1;
	server_addr.u8[1] = 0;
	SENSORS_ACTIVATE(button_sensor);
	unicast_open(&uconn,146,&unicast_callbacks);
	broadcast_open(&bconn,129,&broadcast_callbacks);
	static struct etimer et;
	etimer_set(&et, CLOCK_SECOND*linkaddr_node_addr.u8[0]*2);
	while(1){
		PROCESS_WAIT_EVENT();
		if(etimer_expired(&et)){
			remove_points();
			discover();
			etimer_set(&et, CLOCK_SECOND*DISCOVER_INTERVAL);
		}
	}
	PROCESS_END();
}

PROCESS_THREAD(close_valve, ev, data){
        PROCESS_BEGIN();
        static struct etimer valve_timer;
        etimer_set(&valve_timer, CLOCK_SECOND*VALVE_TIME);
//        printf("startgin valve process\n");
        //index of the node for which to close the valve
        int index;
        while(1){
            PROCESS_WAIT_EVENT();
            if(etimer_expired(&valve_timer)){
                //printf("valve timer expired, closing %d\n", computated[index].u8[0]);
                send_data(&computated[index], SENSOR_CLOSE);
                PROCESS_EXIT();
            }
            else if(ev==PROCESS_EVENT_CONTINUE){
                index = *(int*)data;
                //printf("received msg, node = %d, ptr = %p\n", index, data);
            }
        }
        PROCESS_END();
}

