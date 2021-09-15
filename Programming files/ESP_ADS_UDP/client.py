import time
import socket

client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
message = b'test'
addr = ("127.0.0.5", 5101) # This is what it should be done

start = time.time()
client_socket.sendto(message, addr)
    
for pings in range(10):
    client_socket.settimeout(0.01)
    try:
        data, server = client_socket.recvfrom(1024)
        end = time.time()
        elapsed = end - start
        print(f'{data} {pings} {elapsed}')
    except socket.timeout:
        print('REQUEST TIMED OUT')