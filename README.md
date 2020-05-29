# EmbeddedCooja
Embedded devices project UCL

You need Cooja on Contiki 3 to run this project.
Installation:
1. Put the c and h files in the contiki folder
2. Download serial2pty here and follow the installation instructions: https://github.com/cmorty/cooja-serial2pty

Running
1. Start your Contiki VM
2. Run Cooja by cd into /home/user/contiki/tools/cooja and typing ant run
3. Create a new simulation
4. Add a z1 mote with the border.c file (this will be the border gateway)
5. Optionnally add one or more z1 mote with the node_c.c file (these will be computation nodes)
6. Add one or more z1 motes with the node_t.c (these will be normal sensor nodes)
7. Activate the serial2pty on the border gateway and note the serial device's number
8. Run python3 server.py x (where x is the /dev/pts/x)
9. Start the simulation
