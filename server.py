import sys
import argparse
import os
import time
import re
from threading import Thread


SENSOR_OPEN = 7
SENSOR_CLOSE = 8
THRESHOLD = 65
MAX_NBR_SENSOR_VALUES = 30
VALVE_TIME = 600   # Unit -> seconds



class Server:
    def __init__(self, id):
        """
            Constructor.
            argument: -id: the id of virtual folder /dev/pts/
        
        """
        self.path = "/dev/pts/{}".format(id)
        print(self.path)
        self.nodes = {}

    def send_data(self, request):
        """Request sending to the border router via serial communication."""
        with open(self.path, "wb+", buffering=0) as term:
            term.write(request.encode())

        
    def update_node(self, node, value):
        """Update the sensor value list of a node with the last receipt."""
        if node in self.nodes:
            #print("node is known")
            if len(self.nodes[node]) >= MAX_NBR_SENSOR_VALUES:
                self.nodes[node].pop(0)
            self.nodes[node].append(value)
        else:   # If node is unknown by the server
            #print("node unknown")
            self.nodes[node] = [value]
        for n in self.nodes:
            print(str(n) + ": " + str(self.nodes[n]))
           
    def process_node(self, node):
        if(len(self.nodes[node])<2):
            return
        prev = self.leastSquare_prevision(self.nodes[node])

        print(prev)
        if(prev > THRESHOLD):
            self.send_open_valve(node)

    def send_open_valve(self, moteId):
        """Send a message to a mote so that it opens its valve."""
        request = "{}/{}\n".format(moteId, SENSOR_OPEN)
        print("Opening {}\n".format(moteId))
        self.send_data(request)
        # Timer untill the sending of closure message
        process = Thread(target=self.wait_for_close_valve, args=(VALVE_TIME, moteId))
        process.start()
        
    def wait_for_close_valve(self, sec, moteId):
        """Create a Thread in order to send the closing message to a mote."""
        #print("wait during {} sec".format(sec))
        time.sleep(sec)
        self.send_close_valve(moteId)
    
    def send_close_valve(self, moteId):
        """Send a message to a mote so that it closes its valve.""" 
        request = "{}/{}\n".format(moteId, SENSOR_CLOSE)
        print("Closing {}\n".format(moteId))
        self.send_data(request)
                

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
     
     
    def run(self):
        """Main function server. Connexion with border router by Serial2pty."""
        # Opening of Serial device in order to read and write packet from border router.
        with open(self.path, "wb+", buffering=0) as term:
            while 1: 
                c=''
                buf=""
                while c != '\n':
                    c = term.read(1).decode()
                    buf = buf+c
                    #print(term.read(1).decode(), end='')
                    sys.stdout.flush()
                buf = buf.strip('\n');

                if(re.match("^\d{1,2}/\d{1,2}$",buf ) is None):
                    #print("Wrong format")
                    continue

                t = buf.split('/')
                node = int(t[0])
                value = int(t[1])
                self.update_node(node, value)
                self.process_node(node)
                #self.send_data("{}/{}\n".format(node,SENSOR_OPEN))
                #print("node = "+str(node))
                #print("value = "+str(value))
    

def main():
    parser = argparse.ArgumentParser(description="Process")
    parser.add_argument("id", metavar="id", type=int, help="An integer id of serial device is required. eg: 21 for /dev/pts/21.")
    args = parser.parse_args()
    
    server = Server(args.id)
    server.run()
    

if(__name__ == "__main__"):
    main()

