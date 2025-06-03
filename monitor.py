import serial
import io
import subprocess
import time
import logging

#ser = serial.Serial('/dev/ttyACM1', 9600)

SDstate=0

Pstate=0
ACC=0;
HEAD=0;
TAIL=0;
SPARE=0;
voltage=0;
SW1=0;
SW2=0;
key=0;

def read_status(status):
	global Pstate
	global voltage
	global SW1
	global SW2
	global key
	if(status[0] != '{'):
		print(status)
		return
	st = status.strip('{}')
	logging.debug( st)
	b = st.split(',')
	Pstate=int(b[0])
	voltage=float(b[2])
	SW1=int(b[3])
	SW2=int(b[4])
	key=int(b[5].split("}")[0])

def process_status():
	global Pstate
	global SDstate
	if(Pstate==2 and SDstate != Pstate):
		logging.warning("Power Loss!")
	if(Pstate==1 and SDstate==2):
		logging.warning("Power Resume!")
	if(Pstate==3 and SDstate != Pstate):
		logging.warning("Power Loss. Invoking shutdown script!")
		subprocess.run(['sh','/opt/carmonitor/shutdown.sh']);
		#invoke shutdown
	SDstate=Pstate


with serial.Serial() as ser:
	ser.baudrate=9600
	ser.port='/dev/ttyACM0'
	ser.timeout=1
	ser.open()
	ser.write(b's\n') #shut the fuck up
	
	while True:
		ser.write(b'q\n') #query status
		status_str=str(ser.readline(),'ascii')
		read_status(status_str)
		process_status()
		time.sleep(0.5)
