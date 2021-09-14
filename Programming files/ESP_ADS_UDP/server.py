import random
import socket

server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
server_socket.bind(('127.0.0.6', 5101))
server_socket.settimeout(5.0)
try:
    message, address = server_socket.recvfrom(1024)
    while True:
        rand = random.randint(0, 1000)
        message = message.upper()
        if rand >= 998:
            server_socket.sendto(message, address)
except socket.timeout:
    print('Receive nothing from client')