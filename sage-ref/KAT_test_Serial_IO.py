#!/usr/bin/env python3
import serial   # pySerial
import sys
import os
import os.path
import time
import subprocess
STATE_START = 0
STATE_READ_RSQ = 1
STATE_CLOSE_RSQ = 2
STATE_READ_RSP = 3
STATE_CLOSE_RSP = 4
STATE_CHECK_DATA = 5
STATE_STOP = 6
SEED_CODE = 18
MSG_CODE = 19
PK_CODE = 20
SK_CODE = 21
SML_CODE = 22
SM_CODE = 23
STOP_CODE = 24
state_variable = 0
count_pk = 0
PARAMETER = ""
#define file
KAT_FILE_TEST = "KAT_test_Serial_IO.elf"
root_folder = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
result_folder = root_folder +"/build/result/"
start_writefile =0
count = 0
if len(sys.argv) < 2:
  print("Usage: {0} [DEVICE]".format(sys.argv[0]))
  sys.exit(-1)

dev = serial.Serial(sys.argv[1], 57600)

kat_generate_folder = os.getcwd() + "/kat/kat_generate/"



while True:
    if(state_variable == STATE_START):
        x = dev.readline()
        print(x)
        PARAMETER =x.decode().split('-')
        PARAMETER = PARAMETER[1].strip()
        print(PARAMETER)
        FILE_REQ = "FromPython_PQCsignKAT_" + PARAMETER + ".req"
        FILE_RSP = "FromPython_PQCsignKAT_" + PARAMETER + ".rsp"
        os.makedirs(result_folder, exist_ok=True)
        file_req = open(result_folder + FILE_REQ,'w')
        if(x.decode().find(KAT_FILE_TEST) != -1):
            state_variable = STATE_READ_RSQ
    elif(state_variable == STATE_READ_RSQ):
        x = dev.readline()
        print(x.decode())
        file_req.write(x.decode())
        if(x.decode().find("finished\n") != -1):
            state_variable = STATE_CLOSE_RSQ
            file_req.close()
    elif(state_variable == STATE_CLOSE_RSQ):
        state_variable = STATE_READ_RSP
    elif(state_variable == STATE_READ_RSP):
        print("---Writting to the RSP file--\r\n")
        os.makedirs(result_folder, exist_ok=True)
        file_rsp = open(result_folder + FILE_RSP,'w')
#        while state_variable != STATE_CLOSE_RSP:
        with open(result_folder + FILE_REQ, "r") as file_req:
            file_rsp.write("# " + PARAMETER + "\n\n")
            for line in file_req:
                if line.startswith("count"):
                    count = line.strip()
                    file_rsp.write(count + '\n')
                    print(count+ ': done')
                elif line.startswith("seed"):
                    seed = line.strip()
                    file_rsp.write(seed + '\n')
                    # Split the string by '='
                    seed = seed.split('=')
                    seed_str = seed[1].strip()
                    seed = bytes.fromhex(seed_str)
                    data = SEED_CODE.to_bytes(4, 'big')
                    dev.write(data)
                    dev.write(seed)
                    # print('Read data',(dev.read(48)))
                elif line.startswith("mlen"):
                    mlen = line.strip()
                    file_rsp.write(mlen + '\n')
                elif line.startswith("msg"):
                    mlen = 33*(count_pk+1)
                    count_pk = count_pk + 1
                    msg = line.strip()
                    file_rsp.write(msg + '\n')
                     # Split the string by '='
                    msg = msg.split('=')
                    msg_str = msg[1].strip()
                    msg = bytes.fromhex(msg_str)
                    data = MSG_CODE.to_bytes(1, 'big')
                    dev.write(data)
                    dev.write(msg)
                elif line.startswith("pk"):
                    # print("---Waiting read pk data from " + KAT_FILE_TEST + " file\r\n")
                    data = PK_CODE.to_bytes(4, 'big')
                    dev.write(data)
                    pk = dev.readline()
                    file_rsp.write(pk.decode('utf-8'))
                    print('PK: done')
                elif line.startswith("sk"):
                   #print("---Waiting read sk data from " + KAT_FILE_TEST + " file\r\n")
                    data = SK_CODE.to_bytes(4, 'big')
                    dev.write(data)                    
                    sk = dev.readline()
                    file_rsp.write(sk.decode('utf-8'))
                    print('SK: done')    
                elif line.startswith("sml"):
                    #print("---Waiting read sml data from " + KAT_FILE_TEST + " file\r\n")
                    data = SML_CODE.to_bytes(4, 'big')
                    dev.write(data)                    
                    sml = dev.readline()
                    file_rsp.write(sml.decode('utf-8'))
                elif line.startswith("sm"):
                    #print("---Waiting read sm data from " + KAT_FILE_TEST + " file\r\n")
                    data = SM_CODE.to_bytes(4, 'big')
                    dev.write(data)                    
                    sm = dev.readline()
                    file_rsp.write(sm.decode('utf-8'))
                    print('SM: done\n')
                elif line.startswith("finished"):
                    data = STOP_CODE.to_bytes(4, 'big')
                    dev.write(data)
                    finish = dev.readline()
                    state_variable = STATE_CLOSE_RSP
                    print(finish.decode('utf-8'))
                    print("finished from Python")
                    file_rsp.close()
    elif(state_variable == STATE_CLOSE_RSP):
        state_variable = STATE_CHECK_DATA
    elif(state_variable == STATE_CHECK_DATA):
        state_variable = STATE_STOP
        print("-------------------Checking data with SAGE-------------------\r\n")
        command = [
            'sage',
            root_folder + '/sage-ref/KAT_check.sage',
            result_folder + "/" + FILE_RSP,
            PARAMETER
        ]
        try:
            subprocess.run(command, check=True)
            print("Test Done!")
        except subprocess.CalledProcessError as e:
            print(f"Error: {e}")
    elif(state_variable == STATE_STOP):
        pass


