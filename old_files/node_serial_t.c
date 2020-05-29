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

static linkaddr_t my_parent;
static linkaddr_t children[MAX_CHILDREN];
static int neighbours[MAX_NEIGHBOURS];
static linkaddr_t route[MAX_NEIGHBOURS];
static linkaddr_t server_addr;

int num_children = 0;
int num_neighbours = 0;
int has_parent = 0;

PROCESS(unicast_process, "unicast process");
AUTOSTART_PROCESSES(&unicast_process);

static packet unserialize(char* string){
	packet pck;
	printf("%s\n", string);
	int u0 = string[0]-'0';
	int u1 = string[1]-'0';
	//printf("u1 = %d\n",u1);
	pck.src.u8[0] = (u0*10)+u1;
	u0 = string[2]-'0';
	u1 = string[3]-'0';
	pck.dst.u8[0] = (u0*10)+u1;
	pck.type = ((string[4]-'0')*10)+(string[5]-'0');
	string += 6;
	//printf("s+6: %s\n", string);
	//strcpy(pck.message,string);
	//printf("%d to %d\n",pck.src.u8[0],pck.dst.u8[0]);
	//printf("type %d\n", pck.type);
	//printf("len %d\n",strlen(string));
	printf("uns m: %s\n", pck.message);
}
static char* serialize(packet* pck){
	char string[128] = "";
	if(pck->src.u8[0]>9){
		sprintf(string+strlen(string),"%d",pck->src.u8[0]);
	}
	else{
		sprintf(string+strlen(string),"0%d",pck->src.u8[0]);	
	}
	if(pck->dst.u8[0]>9){
		sprintf(string+strlen(string),"%d",pck->dst.u8[0]);
	}
	else{
		sprintf(string+strlen(string),"0%d",pck->dst.u8[0]);	
	}
	if(pck->type>9){
		sprintf(string+strlen(string),"%d",pck->type);
	}
	else{
		sprintf(string+strlen(string),"0%d",pck->type);	
	}
	sprintf(string+strlen(string),"%s",pck->message);
	printf("serialized: %s\n",string);
	return string;
}
static void send_pck_bc(packet* pck){
	char* string;
	strcpy(string,serialize(pck));

	packetbuf_copyfrom(string,sizeof(string));
	broadcast_send(&bconn);
}
static void send_pck(packet* pck, linkaddr_t* to){
	char* string = serialize(pck);
	packetbuf_copyfrom(string,sizeof(string));
	unicast_send(&uconn,to);
}
static packet recv_pck(char* string){
	packet pck = unserialize(string);
	return pck;
}
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
static void add_neighbour(linkaddr_t* addr){
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
		//printf("pls i am orphan\n");
	}
	pck.dst = *disc;
	pck.src = linkaddr_node_addr;
	//pck.message = "hello je t'entends\n";
	//strcpy(pck.message,  "hello je t'entends");
	send_pck(&pck,&pck.dst);

}

static void recv_broadcast(struct broadcast_conn *conn, const linkaddr_t *from){
	//printf("broadcast received from %d.%d\n",from->u8[0],from->u8[1]);
	char* string = (char*) packetbuf_dataptr();
	printf("s%s\n",string);
	packet pck = recv_pck(string);
	if(pck.type == DISCOVER){
		recv_discover(&pck.src);
	}
}

static void sent_broadcast(struct broadcast_conn *conn, int status, int num_tx){
	//printf("Broadcast sent: status %d num_tx: %d\n",status,num_tx);
}


//remove parent if he is no longer a neighbour
static void remove_dead_parent(){
	
	if(!contains_nb(&my_parent)){
		//parent is dead :(
		has_parent = 0;
		my_parent = linkaddr_null;
		printf("my parent is dead, I am now orphan\n");
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
	//pck.message = "I want to adopt you\n";
	//strcpy(pck.message, "I want to adopt you\n");
	printf("trying to adopt %d\n",child->u8[0]);
	send_pck(&pck,child);
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
		//pck_ack.message = "You adopted me\n";
		//strcpy(pck_ack.message,"You adopted me\n");
		has_parent = 1;
		my_parent = *parent;
		
		send_pck(&pck_ack,parent);
		printf("sending confirmation to %d\n",parent->u8[0]);
	}
	else{
		printf("already have a parent\n");
	}
}
//Sending a broadcast to discover a node's neighbours
static void trim_neighbours(){
	int i;
	for(i = 0;i<MAX_NEIGHBOURS;i++){
		if(neighbours[i]>0){
			neighbours[i]--;
		}
	}
}
static void discover(){
	print_neighbours();
	print_children();
	print_parent();
	print_route();
	trim_neighbours();
	remove_dead_children();
	if(has_parent){
		remove_dead_parent();
	}

	/*
	tableau de voisins
	[0] = 0
	[1] = 0
	[2] = 0
	[3] = 0
	[4] = 3
	[30] = 3
	{0: 0,30: 3, 4:3}
	tout 


	*/



	//clear neighbours list, to refill with actual neighbours	
	num_neighbours = 0;
	//quand appeler ces fonctions? idealement apres avoir recu tous les HELLO des voisins, mais comment le savoir?
	packet pck;
	pck.type = DISCOVER;
	pck.src = linkaddr_node_addr;
	pck.dst = linkaddr_null;
	//pck.message = "Est-que qqn m'entend?\n";
	//strcpy(pck.message, "Est-que qqn m'entend?\n");
	send_pck_bc(&pck);
}
static void add_route(linkaddr_t* dst, linkaddr_t* child){
	route[dst->u8[0]] = *child;
	printf("Route %d, send to %d\n", dst->u8[0], child->u8[0]);
}

//received an adoption confirmation (PARENT_ACK) from a child
//insert him in my children list
static void confirm_adoption(linkaddr_t* child){
	printf("confirming addoption of %d\n",child->u8[0]);
	add_route(child, child);
	insert_addr(child,children,&num_children);
	printf("%d is now my child\n",child->u8[0]);
}
static void send_data(){
	packet pck;
	pck.src = linkaddr_node_addr;
	printf("%d is me\n",pck.src.u8[0]);
	pck.dst = server_addr;
	pck.type= SENSOR_DATA;
	//pck.message="this is a message\n";
	//strcpy(pck.message, "this is a message\n");
	send_pck(&pck, &server_addr);
}
static void relay_parent(packet* pck, linkaddr_t* from){
	add_route(&pck->src, from);
	send_pck(pck,&my_parent);
	linkaddr_t s = pck->src;
	printf("from %d relay to parent %d\n",s.u8[0],my_parent.u8[0]);
}
static void recv_unicast(struct unicast_conn *conn, const linkaddr_t *from){
	char* string = (char*)packetbuf_dataptr();
	packet rpck = recv_pck(string);
	packet* pck = &rpck;
	add_neighbour(from);
	if(linkaddr_cmp(&pck->dst,&linkaddr_node_addr)){
		if(pck->type==SENSOR_DATA){
			printf("received message from %d\n",pck->src.u8[0]);
			printf(pck->message);
		}
		else if(pck->type==PARENT_ACK){
			confirm_adoption(&pck->src);
		}
		else if(pck->type==HELLO_ORPHAN || pck->type==HELLO_CHILD){
			recv_hello(pck);
		}
		if(pck->type == CHILD_HELLO){
			get_adopted(&pck->src);
		}
	}
	else{
		relay_parent(pck, from);
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
	static struct etimer et;
	etimer_set(&et, CLOCK_SECOND*linkaddr_node_addr.u8[0]*2);
	while(1){
		PROCESS_WAIT_EVENT();
		if(ev == sensors_event){

			printf("button pressed\n");
			//send_data();
			/*
			packet pck;
			pck.src = linkaddr_node_addr;
			pck.dst = server_addr;
			pck.type = HELLO_CHILD;
			strcpy(pck.message, "myMessage");
			char* str;
			strcpy(str,serialize(&pck));
			//printf("ser %s\n",str);
			unserialize(str);
			*/
			discover();
		}
		if(etimer_expired(&et)){
		//	discover();
			etimer_set(&et, CLOCK_SECOND*20);
		}
	}
	PROCESS_END();
}


