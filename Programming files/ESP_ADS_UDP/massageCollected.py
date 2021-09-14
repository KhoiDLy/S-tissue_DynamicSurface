 #!/usr/bin/env python3

import struct
import sys
import csv
import matplotlib.pyplot as plt
import numpy as np

NUM_ADC= 2

for i in range(11):
    datafile = open('collect_parallel_'+ str(i)+'.csv','r')
    datainfo= str(datafile.readlines()[0])
    datai= datainfo.split(",")
    data= [float(i) for i in datai]
    
    Data= np.array(data)
    print(Data.shape)
    
    if len(Data) % 2 == 0:
        Data= np.reshape(Data, (-1, NUM_ADC))
    else:
        Data= np.reshape(Data[:-1], (-1, NUM_ADC))
    
    fig = plt.figure()    
    plt.plot(Data[:,0], label = 'Channel 1')   
    
    plt.legend()
    fig.suptitle('Data from ESP'+str(i))
    plt.xlabel('Number of data')
    plt.ylabel('Raw Values')
    fig.savefig('ESP_'+ str(i) +'.jpg')
    plt.show()
    
    fig = plt.figure()   
    plt.plot(Data[:,1], label = 'Channel 2')
    plt.legend()
    fig.suptitle('Data from ESP'+str(i))
    plt.xlabel('Number of data')
    plt.ylabel('Raw Values')
    fig.savefig('ESP_'+ str(i) +'.jpg')
    plt.show()