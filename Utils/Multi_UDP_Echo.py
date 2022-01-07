# Multiple UDP Echo Server
# Based on code by David Manouchehri
# Threading added by PaulZC

#!/usr/bin/env python2
# -*- coding: utf-8 -*-

# Original author: David Manouchehri <manouchehri@protonmail.com>
# This script will always echo back data on the UDP port of your choice.
# Useful if you want nmap to report a UDP port as "open" instead of "open|filtered" on a standard scan.
# Works with both Python 2 & 3.

import socket
import threading

num_ports = 5 # Echo on this many ports starting at server_port_base
server_port_base = 1200 # Change this if required
server_address = '192.168.0.50' # Change this to match your local IP

def echo(address, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    serv = (address, port)
    sock.bind(serv)
    while True:
        payload, client_address = sock.recvfrom(4) # Try to receive 4 bytes
        print("Port " + str(port) + ": Echoing 4 bytes back to " + str(client_address))
        sent = sock.sendto(payload, client_address)
        
if __name__ == "__main__":
    print("Listening on " + server_address)

    threads = list()

    for i in range(num_ports):
        threads.append( threading.Thread( \
            target = echo, args = (server_address, server_port_base + i)) )
        threads[i].daemon = True
        threads[i].start()

	
