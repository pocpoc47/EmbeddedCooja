import sys
import argparse
import os
import time
from threading import Thread


SENSOR_OPEN = 7
SENSOR_CLOSE = 8
THRESHOLD = 0.3
MAX_NBR_SENSOR_VALUES = 30
TIME_VALVE_OPEN = 0.5   # Unit -> seconds

class Server:
    def __init__(self, id):
        """
            Constructor.
            argument: -id: the id of virtual folder /dev/pts/
        
        """
        self.path = "/dev/pts/{}".format(id)
        print(self.path)
        self.nodes = {}

    def run(self):
        """Main function server. Connexion with border router by Serial2pty."""
        # Opening of Serial device in order to read and write packet from border router.
        with open(self.path, "wb+", buffering=0) as term:
            while True:
                print(term.read(1).decode(), end='')
                sys.stdout.flush()
        
    def update_node(self, node, value):
        """Update de sensor value list of a node with de last receipt."""
        if node in self.nodes:
            if len(node) == MAX_NBR_SENSOR_VALUES:
                node.pop(0)
                node.append(value)
        else:   # If node is unkwonw by the server
            self.nodes[node] = [value]
           
    def send_open_valve(self, moteId):
        """Send a message to a mote so that it opens its valve."""
        request = "{}/{}\n".format(moteId, SENSOR_OPEN)
        self.send_data(request)
        # Timer untill the sending of closure message
        process = Thread(target=wait_for_close_valve, args=(TIME_VALVE_OPEN, moteId))
        process.start()
        
    def wait_for_close_valve(self, sec, moteId):
        """Create a Thread in order to send the closing message to a mote."""
        #print("wait during {} sec".format(sec))
        time.sleep(sec)
        self.send_close_valve(moteId)
    
    def send_close_valve(self, moteId):
        """Send a message to a mote so that it closes its valve.""" 
        request = "{}/{}\n".format(moteId, SENSOR_CLOSE)
        self.send_data(request)
                
    def send_data(self, request):
        """Request sending to the border router via serial communication."""
        with open(self.path, "wb+", buffering=0) as term:
            term.write(request.encode())

    def leastSquare_prevision(self, y_values):
        """
            Least Square method.
            argument: -y_values: the last thirty sensor values published on a node
        """
        prevision = None
        x_values = [xi+1 for xi in range(len(y_values))]
        # variable declaration
        xi_minus_x_avg ,yi_minus_y_avg, xi_minus_x_avg_square, product_xi_minus_x_avgANDyi_minus_y_avg = [], [], [], []
        # averaging
        x_avg = sum(x_values) / len(x_values)
        y_avg = sum(y_values) / len(y_values)
        # (xi - x_avg)
        for xi in x_values:
            xi_minus_x_avg.append(xi - x_avg)
        # (yi - y_avg)
        for yi in y_values:
            yi_minus_y_avg.append(yi - y_avg)
        # (xi - x_avg)²
        for xi in x_values:
            xi_minus_x_avg_square.append((xi - x_avg)**2)
        # (xi - x_avg)(yi - y_avg)
        for elem1, elem2 in zip(xi_minus_x_avg, yi_minus_y_avg):
            product_xi_minus_x_avgANDyi_minus_y_avg.append(elem1 * elem2)
        # calculation of the adjustment slope
        a = sum(product_xi_minus_x_avgANDyi_minus_y_avg) / sum(xi_minus_x_avg_square)
        # calculation of the ordinate at the origin of the adjustment line
        b = y_avg - a*x_avg
        # prevision of next value
        x = len(y_values) + 1
        prevision = a*x + b
        return prevision
     
     
    

def main():
    parser = argparse.ArgumentParser(description="Process")
    parser.add_argument("id", metavar="id", type=int, help="An integer id of serial device is required. eg: 21 for /dev/pts/21.")
    args = parser.parse_args()
    
    server = Server(args.id)
    server.run()
    

if(__name__ == "__main__"):
    main()

