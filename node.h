#define CHILD_HELLO 1
#define PARENT_ACK 2
#define SENSOR_DATA 3
#define DISCOVER 4
#define HELLO_ORPHAN 5
#define HELLO_CHILD 6


#define MAX_CHILDREN 4
#define MAX_NEIGHBOURS 10
static struct unicast_conn uconn;
static struct broadcast_conn bconn;

//static linkaddr_t server_addr;





typedef struct packet{
	linkaddr_t src;
	linkaddr_t dst;
	int type;
	char* message;
}packet;

