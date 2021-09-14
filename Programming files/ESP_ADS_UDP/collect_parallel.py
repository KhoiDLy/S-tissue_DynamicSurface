import struct
import socket
import sys
import time
import os
from multiprocessing import Pool, Manager, Value, Queue
# Pool --> pool of cores
# Manager--> scheduler capable of making locks
# Value--> I forgot
# Queue--> allows you to share data across processes


NUM_ESP= 11
BUFFER_SIZE= 10000000000
STARTING_UDP_PORT= 5101
import datetime


def collection_function(ready_to_read, IP, UDP_PORT, espID):
#def collection_function(x):
    # 'q' is optional. However, I added it because most likely you will need
    # to share some sort of data structure across processes
    # go to main to see how to declare it. In this callback
    # function you can read and write in q from any process
    # However, this is a double-edged sword because you
    # have to work so you have a good idea of who wrote what
    # and when.
    var = 1
    filename= ('collect_parallel'+ '_' + str(espID)+'.csv')
    sock= socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_addr = ( IP, UDP_PORT)
#    sock.connect((IP, UDP_PORT))
#    message = b'te'
#    sock.sendto(message, (IP, UDP_PORT))
    
    print("Connection established for esp # " + str(espID))
    try:
        listl=[]

        while (ready_to_read):
            GlobalCommand = struct.pack('1I', var)
            sock.sendto(GlobalCommand, server_addr)
#            now = datetime.datetime.now()
#            print("Current microsecond: %d" % now.microsecond)
            var +=1
#            print("var is ",var)

            sock.settimeout(0.05)
            try:
                (data, server) = sock.recvfrom(BUFFER_SIZE)
                str1= str(len(data)//4) + "I"
                data= struct.unpack(str1, data)
                listl+= list(data)
            except socket.timeout:
                print('REQUEST TIMEOUT')
                
    except KeyboardInterrupt:
        print("saving data file for esp #"+ str(espID))
        filef= open(filename, 'w')
        listl= ",".join(str(bit) for bit in listl)
        filef.write(listl)
        sock.close()

if __name__ == "__main__":
    print("Trying to Connect to PZTs")
    manager = Manager()
    d = manager.dict() #the processes will be sharing a dictionary (optional)
        
    ESPIPlist={}
    
    # These were the IPs that I needed to connect
    # change these to the ones you need
    
    
    ESPIPlist[0]='192.168.1.5'
    ESPIPlist[1]='192.168.1.6'
    ESPIPlist[2]='192.168.1.7'
    ESPIPlist[3]='192.168.1.8'
    ESPIPlist[4]='192.168.1.9'
    ESPIPlist[5]='192.168.1.10'
    ESPIPlist[6]='192.168.1.11'
    ESPIPlist[7]='192.168.1.12'
    ESPIPlist[8]='192.168.1.15'
    ESPIPlist[9]='192.168.1.14'
    ESPIPlist[10]='192.168.1.3'
    
    ready_to_read= manager.Value('ready_to_read', False) # example of using semaphores across all processes
    ready_to_read.value= False
    
    pool= Pool(processes=NUM_ESP)
    results=[] #this is optional, you can just apply_async but this worked for me

    # Setting up the processes, each ESP32 will be handled by a process (there are total 12 processes)
    for count in range(NUM_ESP):
        results.append(
        pool.apply_async(collection_function, (ready_to_read, ESPIPlist[count], STARTING_UDP_PORT+count, count))
        )

    pool.close() # prevent any more tasks from being submitted to the pool. This does not mean that the multiprocessing is closed! It will automatically close when the tasks are done

    ready_to_read.value=True # Now the processes are running in the background. When we turn the ready_to_read TRUE, the sock.recv will begin


    for result in results:
        result.get()

    pool.join()
