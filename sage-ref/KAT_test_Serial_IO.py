#!/usr/bin/env python3
import serial   # pySerial
import sys
import os
import os.path
import time
import subprocess
#Declare variable
state_variable = 0
count_pk = 0
root_folder = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
result_folder = root_folder +"/build/result/"
start_writefile =0
count = 0
#Define File
KAT_FILE_TEST = "KAT_test_Stream.elf"
PARAMETER = ""
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
MEDS_S = 6
MEDS_t = 112
key_gen_result = dict()
count_line = 0
if len(sys.argv) < 2:
  print("Usage: {0} [DEVICE]".format(sys.argv[0]))
  sys.exit(-1)

dev = serial.Serial(sys.argv[1], 500000) #57600

kat_generate_folder = os.getcwd() + "/kat/kat_generate/"

def do_get_data_from_keygen(PARAMETER,MEDS_s,FILE,count_line):

    if PARAMETER not in key_gen_result:
        key_gen_result[PARAMETER] = {"G0_data": [], "G_data": [], "A_inv_data": [],"A_tilde_data": [],"B_inv_data": [],"B_tilde_data": [],"sk_data": [],"pk_data": [],"sm_data": []}

    dev.read(1)
    key_gen_result[PARAMETER]["G0_data"] = dev.readline().strip().decode('utf-8')
    dev.write('1'.encode())

    FILE.write(f"G0_data = " 
                   + key_gen_result[PARAMETER]["G0_data"]+ '\n')

    #Read and Send Raw Data
    for i in range(1,MEDS_s):
        dev.read(1) #receive to send data
        for byte in bytes.fromhex(key_gen_result[PARAMETER]["G0_data"]):
            bytes_written = dev.write(bytes([byte]))  # Write one byte at a time
        key_gen_result[PARAMETER]["A_inv_data"] = dev.readline().strip().decode('utf-8')
        key_gen_result[PARAMETER]["B_inv_data"] = dev.readline().strip().decode('utf-8')
        key_gen_result[PARAMETER]["G_data"] = dev.readline().strip().decode('utf-8')
        #send G0 to re-generating the Gprime
        for byte in bytes.fromhex(key_gen_result[PARAMETER]["G0_data"]):
            bytes_written = dev.write(bytes([byte]))  # Write one byte at a time
        dev.write('1'.encode()) #send for done sending      
        FILE.write(f"A_inv_data {i} = " 
                   + key_gen_result[PARAMETER]["A_inv_data"]+ '\n')
        FILE.write(f"B_inv_data {i} = " 
                   + key_gen_result[PARAMETER]["B_inv_data"]+ '\n')
        FILE.write(f"G_data {i} = " 
                   + key_gen_result[PARAMETER]["G_data"]+ '\n')

    FILE.seek(0) # Set the file pointer at the beginning at file
    #Send back A_inv_data and B_inv_data and Read SK data
    found = False
    line_A = 0
    line_B = 0
    line_G = 0
    #Find A_inv_data line and then send data
    for line in FILE:
        if line.startswith(f"count = {count_line}"):
            found = True
        if found:
            if line.startswith("A_inv_data"):
                line_A = line_A + 1
                dev.read(1)
                A_inv_data = line.strip().split('=')[1].strip()
                for byte in bytes.fromhex(A_inv_data):
                    bytes_written = dev.write(bytes([byte]))
                dev.write('1'.encode())
                if(line_A == (MEDS_s-1)):
                    found = False
    FILE.seek(0) # Set the file pointer at the beginning at file
    #Find B_inv_data line and then send data
    for line in FILE:
        if line.startswith(f"count = {count_line}"):
            found = True
        if found:
            if line.startswith("B_inv_data"):
                line_B = line_B + 1
                dev.read(1)
                B_inv_data = line.strip().split('=')[1].strip()
                for byte in bytes.fromhex(B_inv_data):
                    bytes_written = dev.write(bytes([byte]))
                dev.write('1'.encode())
                if(line_B == (MEDS_s-1)):
                    found = False
    dev.read(1)
    key_gen_result[PARAMETER]["sk_data"] = dev.readline().strip().decode('utf-8')
    dev.write('1'.encode())
    #Send back G data and read pk data
    FILE.seek(0)
    for line in FILE:
        if line.startswith(f"count = {count_line}"):
            found = True
        if found:
            if line.startswith("G_data"):
                line_G = line_G + 1
                dev.read(1)
                G_data = line.strip().split('=')[1].strip()
                for byte in bytes.fromhex(G_data):
                    bytes_written = dev.write(bytes([byte]))
                pk = dev.readline().strip().decode('utf-8')
                key_gen_result[PARAMETER]["pk_data"].append(pk)
                dev.write('1'.encode())
                print(f"---PK and SK processing....{int(line_G*100/MEDS_s)}%----")
                if(line_G == (MEDS_s-1)):
                    found = False
    sk_data = key_gen_result[PARAMETER]["sk_data"]
    pk_data = key_gen_result[PARAMETER]["pk_data"]

    # Reset the pk_data list after returning the values
    key_gen_result[PARAMETER]["pk_data"] = []
    print(f"---PK and SK processing....100%----")
    return sk_data, pk_data

def do_get_data_from_sig(PARAMETER,MEDS_t,FILE_RSQ,count_line,FILE_Result):
    found = False
    line_A = 0
    line_B = 0
    FILE_RSQ.seek(0)
    for line in FILE_RSQ:
        if line.startswith(f"count = {count_line}"):
            found = True
        if found:
            if line.startswith("sk = "):
                sk = line.strip().split('=')[1].strip()
                dev.read(1)
                for byte in bytes.fromhex(sk):
                    bytes_written = dev.write(bytes([byte]))
                dev.write('1'.encode())

    dev.read(1)
    key_gen_result[PARAMETER]["G0_data"] = dev.readline().strip().decode('utf-8')
    dev.write('1'.encode())

    FILE_Result.write(f"G0_data = " 
                   + key_gen_result[PARAMETER]["G0_data"]+ '\n')

    print(f"---SM processing-----")
    for i in range(0,MEDS_t):
        dev.read(1) #receive to send data
        #send G0 to re-generating the Gprime
        key_gen_result[PARAMETER]["A_tilde_data"] = dev.readline().strip().decode('utf-8')
        key_gen_result[PARAMETER]["B_tilde_data"] = dev.readline().strip().decode('utf-8')
        for byte in bytes.fromhex(key_gen_result[PARAMETER]["G0_data"]):
            bytes_written = dev.write(bytes([byte]))  # Write one byte at a time
        dev.write('1'.encode()) #send for done sending
        if(i%10 ==0):
            print(f"Send G0_tilde_ti....{int(i*100/MEDS_t)}%----")
        FILE_Result.write(f"A_tilde_data {i} = " 
                   + key_gen_result[PARAMETER]["A_tilde_data"]+ '\n')
        FILE_Result.write(f"B_tilde_data {i} = " 
                   + key_gen_result[PARAMETER]["B_tilde_data"]+ '\n')
    print(f"Send G0_tilde_ti....100%----")
    
    FILE_Result.seek(0)
    for line in FILE_Result:
        if line.startswith(f"count = {count_line}"):
            found = True
        if found:
            if line.startswith("A_tilde_data"):
                line_A = line_A + 1
                dev.read(1)
                A_tilde_data = line.strip().split('=')[1].strip()
                for byte in bytes.fromhex(A_tilde_data):
                    bytes_written = dev.write(bytes([byte]))
            if line.startswith("B_tilde_data"):
                line_B = line_B + 1
                B_tilde_data = line.strip().split('=')[1].strip()
                for byte in bytes.fromhex(B_tilde_data):
                    bytes_written = dev.write(bytes([byte]))
                sm = dev.readline().strip().decode('utf-8')
                key_gen_result[PARAMETER]["sm_data"].append(sm)
                dev.write('1'.encode())
                if(line_B%10 ==0):
                    print(f"Read SM data....{int(line_B*100/MEDS_t)}%----")
                if((line_B == (MEDS_t))):
                    line_B = 0
                    found = False
    print(f"Read SM data....100%----")

    dev.read(1)
    sm = dev.readline().strip().decode('utf-8')
    key_gen_result[PARAMETER]["sm_data"].append(sm)
    dev.write('1'.encode())
    
    sm_data = key_gen_result[PARAMETER]["sm_data"]
    key_gen_result[PARAMETER]["sm_data"] = []
    return sm_data
if __name__ == "__main__":
    while True:
        if(state_variable == STATE_START):
            dev.write('1'.encode())
            x = dev.readline()
            print(x)
            PARAMETER =x.decode().split('-')
            PARAMETER = PARAMETER[1].strip()
            print(PARAMETER)
            FILE_REQ = "FromPython_PQCsignKAT_" + PARAMETER + ".req"
            FILE_RSP = "FromPython_PQCsignKAT_" + PARAMETER + ".rsp"
            FILE_DATA_Keygen = "FromPython_Result_Keygen_" + PARAMETER + ".txt"
            FILE_DATA_Sig = "FromPython_Result_Sig_" + PARAMETER + ".txt"
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
            #*****SK,PK,SM,SMLEN DATA******#
            sk = 0
            pk = 0
            sm = 0
    #        while state_variable != STATE_CLOSE_RSP:
            with open(result_folder + FILE_REQ, "r") as file_req, \
                open(result_folder + FILE_DATA_Keygen,'w+') as file_result_keygen, \
                open(result_folder + FILE_DATA_Sig,'w+') as file_result_sig, \
                open(result_folder + FILE_RSP,'w+') as file_rsp:
                
                file_rsp.write("# " + PARAMETER + "\n\n")

                for line in file_req:
                    if line.startswith("count"):
                        count = line.strip()
                        file_rsp.write(count + '\n')
                        file_result_keygen.write(count + '\n')
                        count_line = count_line + 1
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
                        data = MSG_CODE.to_bytes(4, 'big')
                        dev.write(data)
                        dev.read(1)
                        dev.write(msg)
                    elif line.startswith("pk"):
                        data = PK_CODE.to_bytes(4, 'big')
                        dev.write(data)
                        sk,pk = do_get_data_from_keygen(PARAMETER,MEDS_S,file_result_keygen,(count_line-1))
                        pk_check = dev.readline()
                        file_rsp.write("pk = ")
                        for pk_data in pk:
                            file_rsp.write(pk_data)
                        file_rsp.write("\n")
                        print('PK: done')
                    elif line.startswith("sk"):
                        data = SK_CODE.to_bytes(4, 'big')
                        dev.write(data)                    
                        sk_check = dev.readline()
                        file_rsp.write(sk+'\n')
                        print('SK: done')    
                    elif line.startswith("sml"):
                        #print("---Waiting read sml data from " + KAT_FILE_TEST + " file\r\n")
                        data = SML_CODE.to_bytes(4, 'big')
                        dev.write(data)
                        sm = do_get_data_from_sig(PARAMETER,MEDS_t,file_rsp,(count_line-1),file_result_sig)                    
                        sml = dev.readline()
                        file_rsp.write(sml.decode('utf-8'))
                    elif line.startswith("sm"):
                        #print("---Waiting read sm data from " + KAT_FILE_TEST + " file\r\n")
                        data = SM_CODE.to_bytes(4, 'big')
                        dev.write(data)                    
                        sm_check = dev.readline()
                        file_rsp.write("sm = ")
                        for sm_data in sm:
                            file_rsp.write(sm_data + "\n")
                        file_rsp.write("\n")
                        print('SM: done\n')
                    elif line.startswith("finished"):
                        data = STOP_CODE.to_bytes(4, 'big')
                        dev.write(data)
                        finish = dev.readline()
                        state_variable = STATE_CLOSE_RSP
                        print(finish.decode('utf-8'))
                        print("finished from Python")
                        file_rsp.close()
                        file_result_keygen.close()
                        file_req.close()
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