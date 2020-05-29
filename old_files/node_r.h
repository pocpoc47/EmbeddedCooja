#define CHILD_HELLO 1
#define PARENT_ACK 2
#define SENSOR_DATA 3
#define DISCOVER 4
#define HELLO_ORPHAN 5
#define HELLO_CHILD 6


#define MAX_CHILDREN 3
#define MAX_TX 5
static struct runicast_conn ruconn;
static struct broadcast_conn bconn;

//static linkaddr_t server_addr;






typedef struct packet{
	linkaddr_t src;
	linkaddr_t dst;
	int type;
	char* message;
}packet;

