#include <HID-Project.h>
#include <HID-Settings.h>
#define LAYOUT_US_ENGLISH

#include <EEPROM.h>

//this is a clusterfuck that no one should use ever
//you need the HID Project library

#define ACC_RELAY 7
#define ACC_DETECT 15 
#define ACC_VOLTAGE A3 //vdiv analog pin
#define ACC_R1 39000.0 //upper resistor on the voltage divider
#define ACC_R2 10000.0 //lower

//how many milliseconds before we send a shutdown command on ACC loss
#define ACC_THRESHHOLD 5000
//how many milliseconds before we shut down anyway
#define ACC_SHUTDOWN 30000

#define TAIL_DETECT 14
#define HEAD_DETECT 16
#define SPARE_DETECT 10

#define SPK_RELAY 6

//Pins for the steering wheel interface
#define SW1 A0
#define SW2 A1

int b1l[] = {1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023};
int b2l[] = {1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023};
int b1h[] = {1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023};
int b2h[] = {1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023};
int kys[] = {
  MEDIA_VOLUME_UP,
  MEDIA_VOLUME_DOWN,
  MEDIA_NEXT,
  MEDIA_PREV,
  MEDIA_VOLUME_MUTE,
  MEDIA_PLAY_PAUSE,
  HID_KEYBOARD_UPARROW,
  HID_KEYBOARD_DOWNARROW,
  HID_KEYBOARD_LEFTARROW,
  HID_KEYBOARD_RIGHTARROW,
  HID_KEYBOARD_ENTER,
  HID_KEYBOARD_ESCAPE,
  0,
  0,
  0,
  0};//the last zero is special for alt+tab
char names[16][4]={"vol+","vol-","next","prev","mute","play"," up ","down","left","rite","entr","esc ","usr1","usr2","usr3","mode"};


uint8_t timer=0; //timer variable to quiet down serial traffic
float accv=0.0; //acc measured voltage keeping this global so we can read it less often
long shutdownTime=0; //when are we shutting down
uint8_t shutdownState=0;
//0=startup
//1=normal
//2=powerloss pre-threshhold
//3=powerloss post-threshhold
//4=we should be shut down but power is still here

uint8_t serstate=0;
//0= normal
//1= wait for button
//2= button down
//3= wait for release
uint8_t progbutt=0;
long progtime=0;

bool stfu=false;
int activeButton=0;
int buttonState=0;

void eeprom_write() {
  Serial.println("EEPROM Written!");
  int a=0, s=sizeof(a);
  EEPROM.put(a,42);
  a+=s;
  EEPROM.put(a,69);
  a+=s;
  EEPROM.put(a,b1l);
  a+=sizeof(b1l);
  EEPROM.put(a,b2l);
  a+=sizeof(b1l);
  EEPROM.put(a,b1h);
  a+=sizeof(b1l);
  EEPROM.put(a,b2h);
  a+=sizeof(b1l);
  EEPROM.put(a,kys);
  a+=sizeof(b1l);
}

void eeprom_reset() {
  EEPROM.write(0,0xFF);
  EEPROM.write(1,0xFF);
  EEPROM.write(2,0xFF);
  EEPROM.write(3,0xFF);
}

void eeprom_read() {
  Serial.print("Reading EEPROM:");
  int g1=0,g2=0,a=0,s=sizeof(g1);

  EEPROM.get(a,g1);
  a+=s;
  EEPROM.get(a,g2);
  a+=s;
  Serial.print(g1);
  Serial.print(",");
  Serial.print(g2);
  if(g1==42 && g2==69) {
    Serial.print(" OK!");
    EEPROM.get(a,b1l);
    a+=sizeof(b1l);
    EEPROM.get(a,b2l);
    a+=sizeof(b1l);
    EEPROM.get(a,b1h);
    a+=sizeof(b1l);
    EEPROM.get(a,b2h);
    a+=sizeof(b1l);
    EEPROM.get(a,kys);
    a+=sizeof(b1l);
  } else {
    eeprom_write();
  }
}

void setup() {
  pinMode(ACC_RELAY, OUTPUT);
  digitalWrite(ACC_RELAY, LOW);

  pinMode(SPK_RELAY, OUTPUT);
  digitalWrite(SPK_RELAY, LOW);
  
  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);

  pinMode(ACC_DETECT, INPUT_PULLUP);
  pinMode(ACC_VOLTAGE, INPUT);
  pinMode(HEAD_DETECT, INPUT_PULLUP);
  pinMode(TAIL_DETECT, INPUT_PULLUP);
  pinMode(SPARE_DETECT, INPUT_PULLUP);

  // open the serial port:
  Serial.begin(9600);
  // initialize control over the keyboard:
  Keyboard.begin();


  Serial.flush();

  eeprom_read();
}

//0: state
//1: inputs state
//2: voltage
//3: SW1
//4: SW2
//5: keycode
void report() {
  Serial.print("{");
  Serial.print(shutdownState);
  Serial.print(",");
  Serial.print(digitalRead(ACC_DETECT));
  Serial.print(digitalRead(HEAD_DETECT));
  Serial.print(digitalRead(TAIL_DETECT));
  Serial.print(digitalRead(SPARE_DETECT));
  Serial.print(",");
  Serial.print(accv);
  Serial.print(",");
  Serial.print(analogRead(SW1));
  Serial.print(",");
  Serial.print(analogRead(SW2));
  Serial.print(",");
  Serial.print(kys[activeButton]*buttonState,HEX);
  Serial.println("}");
  
  Serial.flush();
}




char serbuff[8];
uint8_t seridx=0;

void progstart() {
  if(progbutt>=16) {//programming complete
    progbutt=0;
    Serial.println("Programming done. send 'e' to store to eeprom.");
    serstate=0;
  } else {
    serstate=1;
    b1h[progbutt]=0;
    b1l[progbutt]=1023;
    b2h[progbutt]=0;
    b2l[progbutt]=1023;
    Serial.print("Press button for ");
    Serial.print(names[progbutt][0]);
    Serial.print(names[progbutt][1]);
    Serial.print(names[progbutt][2]);
    Serial.print(names[progbutt][3]);
    Serial.println(" for 3 seconds or enter to skip.");
  }
}

void debug() {
  Serial.println("KEY TABLE:");
  for(int i=0; i<16; i++) {
    Serial.print(b1l[i]);
    Serial.print(",");
    Serial.print(b2l[i]);
    Serial.print(",");
    Serial.print(b1h[i]);
    Serial.print(",");
    Serial.print(b2h[i]);
    Serial.print(",");
    Serial.println(kys[i]);
  }
}

//Serial commands
//s == shut the fuck up unless queried
//t == talk, don't wait for q
//q == query
//p == program steering wheel
//e == eeprom store
//r == reset eeprom
//o == shutdown override
//n == NO OP
//d == debug. dumps active key table

void menuloop() {
  switch(serbuff[0]) {
    case 's':
      stfu=true;
      break;
    case 't':
      stfu=false;
      break;
    case 'q':
      report();
      break;
    case 'p':
      progbutt=0;//we will maybe pull button numbers for one shots from the serial buffer
      progstart();
      break;
    case 'e':
      eeprom_write();
      break;
    case 'r':
      Serial.println("EEPROM Reset, reboot now.");
      eeprom_reset();
      break;
    case 'd':
      debug();
      break;
    case 'n':
    default:
    Serial.println("ERR:bad choice");
    seridx=0;
  }
  Serial.flush();
}



void loop() {
  // check for incoming serial data and fill the buffer
  while (Serial.available() > 0) {
    char inChar = Serial.read();
    serbuff[seridx]=inChar;
    seridx++;
    if(inChar=='\n') {//attempt to execute command on newline
      if(serstate==0) {
         menuloop(); //we aren't in programming state
      } else {
        b1l[progbutt]=1023;//set empty state so we can avoid that button
        b2l[progbutt]=1023;
        b1h[progbutt]=1023;
        b2h[progbutt]=1023;
        progbutt++;//we've received a newline during programming, abort and go to next button
        progstart();
      }
      seridx=0;
    }
    if(seridx>=7) {//don't overflow error out if buffer is full
      Serial.println("ERR:Overflow");
      seridx=0;
    }
  }

  if(serstate>0) {//if we're in programming mode
    int sw1=analogRead(SW1);
    int sw2=analogRead(SW2);
    switch(serstate) {

      case 1://wait for button
        if(sw1<1000 || sw2<1000) { //activity detected
          delay(100);//stabalize
          serstate++;
          progtime=millis()+3000;//set timer for 3 seconds
        }
        break;
      case 2://button down
        if(sw1<1000 || sw2<1000) {
          if(millis()<progtime) {
            if(sw1>1000) {//if untouched give it pristine values
              b1l[progbutt]=1002;
              b1h[progbutt]=1021;
            } else {
              if(b1l[progbutt]>sw1) b1l[progbutt]=sw1;
              if(b1h[progbutt]<sw1) b1h[progbutt]=sw1;
            }
            if(sw2>1000){
              b2l[progbutt]=1002;
              b2h[progbutt]=1021;
            } else {
              if(b2l[progbutt]>sw2) b2l[progbutt]=sw2;
              if(b2h[progbutt]<sw2) b2h[progbutt]=sw2;
            }
          } else {
            b1l[progbutt]-=2;// a little extra margin
            b1h[progbutt]+=2;
            b2l[progbutt]-=2;
            b2h[progbutt]+=2;

            Serial.print("{");
            Serial.print(b1l[progbutt]);
            Serial.print(",");
            Serial.print(b1h[progbutt]);
            Serial.print(",");
            Serial.print(b2l[progbutt]);
            Serial.print(",");
            Serial.print(b2h[progbutt]);
            Serial.println("} done. Release.");
            serstate=3;
          }
        } else { //premature release
          Serial.println("ERR:Too Soon");
          progstart();
        }
        break;
      case 3://waiting for release
        if(sw1>1000 && sw2>1000) {
          progbutt++;
          progstart();
        }
    }
  } else { //read buttons
    int sw1=analogRead(SW1);
    int sw2=analogRead(SW2);
    for(int i=0;i<16;i++){
      if(b1l[i]==1023 && b2l[i]==1023 && b1h[i]==1023 && b1l==1023) {}//this just prevents unset buttons from being detected when none pressed
        else {
          if(sw1>b1l[i]&&sw1<b1h[i]&&sw2>b2l[i]&&sw2<b2h[i]) { //if in range
            if(buttonState==1){
              if(activeButton!=i) {//going key to key without release inbetween
                Keyboard.release(kys[activeButton]); //unpress old key
                Keyboard.press(kys[i]);
                activeButton=i;
                //do nothing, keep pressed
              } else { //There was no active button
                activeButton=i;
                buttonState=1;
                Keyboard.press(kys[i]);
              }
          }
        }
      }
    }
    if(buttonState==1 && sw1>1000 && sw2>1000) { //no buttons active=release
      Keyboard.release(kys[activeButton]);
      buttonState=0;
    }
  }

  switch(shutdownState) {
    case 0:
      digitalWrite(ACC_RELAY, LOW);
      shutdownState=1;
      shutdownTime=0;
    case 1: //Normal state
      if(digitalRead(ACC_DETECT)) { //check for power loss
        shutdownState=2;//go into threshold state
        shutdownTime=millis() + ACC_THRESHHOLD;
      }
      break;
    case 2: //Threshhold state
      if(!digitalRead(ACC_DETECT)) {//check if power is back
        shutdownState=1;
        shutdownTime=0;
      } else if(millis()>shutdownTime) {
        Serial.print("ACC Power Loss. Shutting down in ");
        Serial.print(ACC_SHUTDOWN);
        Serial.println("ms.");
        shutdownTime=millis() + ACC_SHUTDOWN;
        shutdownState=3;
      }
      break;
    case 3:
      if(millis()>shutdownTime) {
        Serial.println("Shutdown");
        Serial.flush();
        shutdownState=4;
        digitalWrite(ACC_RELAY, HIGH);
      }
      break;
    case 4: // we shouldn't ever be here, but be cool
      //Serial.print(".");
      if(!digitalRead(ACC_DETECT)) {//check if power is back and jump back to normal state
        shutdownState=0;
      }
      break;
  }



  if(timer==0) {
    
    accv=((5.0 * analogRead(ACC_VOLTAGE)/1023) * (ACC_R1 + ACC_R2)) / ACC_R2 ;

    if(!stfu && serstate==0) report();
      
    timer=127;
  }
  timer--;
}
