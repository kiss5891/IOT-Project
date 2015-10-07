#include <PN532.h>
#include <SPI.h>
#include <SoftwareSerial.h>

/*Chip select pin can be connected to D10 or D9 which is hareware optional*/
/*if you the version of NFC Shield from SeeedStudio is v2.0.*/
#define PN532_CS 10
#define BuzzerPin 6
#define DoorPin 7
PN532 nfc(PN532_CS);
#define  NFC_DEMO_DEBUG 1
uint32_t table[]={2948616998};

#include <SoftwareSerial.h>
SoftwareSerial mySerial(4, 5); // RX, TX

void setup(void) {
  pinMode(DoorPin, OUTPUT);
  digitalWrite(DoorPin, HIGH);
  pinMode(3, INPUT);
  pinMode(4, INPUT);
  pinMode(BuzzerPin, OUTPUT);
  digitalWrite(BuzzerPin, LOW);
  
#ifdef NFC_DEMO_DEBUG
  Serial.begin(9600);
  Serial.println("Hello!");
#endif
  nfc.begin();
  mySerial.begin(9600);

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
#ifdef NFC_DEMO_DEBUG
    Serial.print("Didn't find PN53x board");
#endif
    while (1); // halt
  }
#ifdef NFC_DEMO_DEBUG
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); 
  Serial.println((versiondata>>24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); 
  Serial.print((versiondata>>16) & 0xFF, DEC);
  Serial.print('.'); 
  Serial.println((versiondata>>8) & 0xFF, DEC);
  Serial.print("Supports "); 
  Serial.println(versiondata & 0xFF, HEX);
#endif
  // configure board to read RFID tags and cards
  nfc.SAMConfig();
}


void loop(void) {
  uint32_t id;
  String ID;
  byte method;
  // look for MiFare type cards
  id = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A);

  if (id != 0) {
    for (byte k=0;k<100;k++)   //感應逼聲  0.5ms * 100 =50ms
    {
      digitalWrite(BuzzerPin, HIGH);
      delayMicroseconds(250);
      digitalWrite(BuzzerPin, LOW);
      delayMicroseconds(250);
    }
    if (!digitalRead(3))    //註冊  method=2
    {
      method=2;
      ID="r/"+(String) id;
      mySerial.println(ID);
      Serial.println(ID);
    }
    else if (!digitalRead(2))    //取消  method=0
    {
      method=0;
      ID="d/"+(String) id;
      mySerial.println(ID);
      Serial.println(ID);
    }
    else    //比對ID   method=1
    {
      method=1;
      ID="c/"+(String) id;
      mySerial.println(ID);
      Serial.println(ID);
    }
    
    String response= readSerial();
    Serial.println(response);
    
    if (response.indexOf("result=1")>=0)
    {
      BuzzerSuccess();
      if (method==1)
      {
        Serial.println("open door");
        opendoor();
      }
    }
    else
    {
      BuzzerFail(); //失敗
    }
  }
}

void opendoor()
{
  digitalWrite(DoorPin, LOW);
  delay(3000);
  digitalWrite(DoorPin,HIGH);
  mySerial.println('0');    //回傳SERVER 門鎖關閉
}

void BuzzerSuccess()   //蜂鳴器成功
{
  for (byte y=0;y<2;y++)   //逼兩短聲   0.5ms * 100 +50ms =100ms 
  {
    for (byte x=0;x<100;x++)
    {
      digitalWrite(BuzzerPin, HIGH);
      delayMicroseconds(250);
      digitalWrite(BuzzerPin, LOW);
      delayMicroseconds(250);
    }
    delay(50);
  }

}
void BuzzerFail()   //蜂鳴器失敗  逼一短聲一長聲   0.5ms * 300 +50ms =200ms
{
  for (byte x=0;x<100;x++)
  {
    digitalWrite(BuzzerPin, HIGH);
    delayMicroseconds(250);
    digitalWrite(BuzzerPin, LOW);
    delayMicroseconds(250);
  }
  
  delay(50);
  
  for (byte x=0;x<200;x++)
  {
    digitalWrite(BuzzerPin, HIGH);
    delayMicroseconds(300);
    digitalWrite(BuzzerPin, LOW);
    delayMicroseconds(300);
  }
}

String readSerial()
{
  String readline="";

  while (readline=="")
  {
    while (!mySerial.available()){}
    while(mySerial.available())
    {
      char inChar = mySerial.read();
      if(inChar==(0x0D) |inChar==(0x0A)){
        break;
      }
      readline=readline+String(inChar);
      delay(5);
    }
  }
  return readline;
}

