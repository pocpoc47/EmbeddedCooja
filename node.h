static struct unicast_conn uconn;
static struct broadcast_conn bconn;


typedef struct packet{
	linkaddr_t src;
	linkaddr_t dst;
	int type;
	char* message;
}packet;

