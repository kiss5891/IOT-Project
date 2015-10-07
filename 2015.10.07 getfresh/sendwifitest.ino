/*
   Web client sketch for IDE v1.0.1 and w5100/w5200
   Uses POST method.
   Posted November 2012 by SurferTim
*/

#include <SPI.h>
#include <Ethernet.h>

#include <Servo.h>
Servo myservo; // create servo object to control a servo

// 匯入程式庫標頭檔 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>

// Arduino數位腳位6接到1-Wire裝置
#define ONE_WIRE_BUS 6   //溫度
#define door 13
float temp;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#include <SoftwareSerial.h>
SoftwareSerial mySerial(2, 3); // RX, TX
bool senddata = 0,windowstatus=0;  //窗戶狀態
//co2
#define         MG9_PIN                       A0     //define which analog input channel you are going to use
//#define         BOOL_PIN                     5
//#define         DC_GAIN                      (8.5)   //define the DC gain of amplifier
//These two values differ from sensor to sensor. user should derermine this value.
//#define         ZERO_POINT_VOLTAGE           (0.220) //define the output of the sensor in volts when the concentration of CO2 is 400PPM
//#define         REACTION_VOLTGAE             (0.020) //define the voltage drop of the sensor when move the sensor from air into 1000ppm CO2

/*****************************Globals***********************************************/
//float           CO2Curve[3]  =  {2.602,ZERO_POINT_VOLTAGE,(REACTION_VOLTGAE/(2.602-3))};   
                                                     //two points are taken from the curve. 
                                                     //with these two points, a line is formed which is
                                                     //"approximately equivalent" to the original curve.
                                                     //data format:{ x, y, slope}; point1: (lg400, 0.324), point2: (lg4000, 0.280) 
                                                     //slope = ( reaction voltage ) / (log400 –log1000) 

//#include <PN532.h>
//#include <SPI.h>

/*Chip select pin can be connected to D10 or D9 which is hareware optional*/
/*if you the version of NFC Shield from SeeedStudio is v2.0.*/
//#define PN532_CS 9
//PN532 nfc(PN532_CS);
//#define  NFC_DEMO_DEBUG 1

// G SENSOR
int I2C_Address = 0xA7 >> 1; // ADXL345 的 I2C 地址
int X0, X1, Y0, Y1, Z1, Z0;
float X,Y,Z;


byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 192, 168, 0, 150 };                    //Static IP address
byte gateway[] = { 192, 168, 0, 1 };                 //Gateway
byte subnet[] = { 255, 255, 255, 0 };                //Netmask

//Change to your server domain
char serverName[] = "211.23.17.100";

// change to your server's port
int serverPort = 81;
int num=0,timeinterval=15;

// change to the page on that server
char pageName[] = "/api/dev/lock";
char pageNametemp[] = "/api/dev/sensor/temp/";
char pageNameg[] = "/api/dev/sensor/grav/";
char pageNamegas[] = "/api/dev/sensor/gas/";
char pageNamenfc[] = "/api/dev/nfc/";
char pageNamelockset[] = "/api/dev/lock/";

//EthernetServer server(80); //server port
EthernetClient client;
int totalCount = 0; 
// insure params is big enough to hold your variables
char params[32];

// set this to the number of milliseconds delay
// this is 30 seconds
#define delayMillis 30000UL

unsigned long thisMillis = 0;
unsigned long lastMillis = 0;
String readString="";
String ID;
void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  Wire.begin();  //初始化 I2C
  setReg(0x2D, 0xA); // (打開電源, 設定輸出資料速度為 100 Hz)
  myservo.attach(14); // 連接數位腳位14，伺服馬達的訊號線
//  pinMode(BOOL_PIN, INPUT);                        //set pin to input
//  digitalWrite(BOOL_PIN, HIGH);                    //turn on pullup resistors
  
  pinMode(8, OUTPUT);

  Serial.print(F("Starting ethernet..."));
  if(!Ethernet.begin(mac)) Serial.println(F("failed"));
  else Serial.println(Ethernet.localIP());
  sensors.begin();  //初始化DS18B20
  sensors.requestTemperatures();
  delay(2000);
  Serial.println(F("Ready"));

  settimerinterrupt();
}

void loop()
{
  readmySerial();
  if ((temp>30 ||analogRead(MG9_PIN)>200) && windowstatus==0)  //可燃氣體大於100，窗戶關閉狀態
  {
    Serial.print("temp");
    Serial.println(temp);
    Serial.print("gas");
    Serial.println(analogRead(MG9_PIN));
    Serial.println(windowstatus);
    for(int i = 0; i <= 90; i+=1)
    {
      myservo.write(i); // 使用write，傳入角度，從0度轉到180度
      delay(2);
    }
    windowstatus=1;
  }
  else if (temp<30 &&analogRead(MG9_PIN)<100 && windowstatus==1)
  {
    Serial.print("temp");
    Serial.println(temp);
    Serial.print("gas");
    Serial.println(analogRead(MG9_PIN));
    Serial.println(windowstatus);
    for(int i = 90; i >= 0; i-=1)
    {
      myservo.write(i); // 使用write，傳入角度，從0度轉到180度
      delay(2);
    }
    windowstatus=0;
  }
}

void settimerinterrupt()  //中斷設定
{
  TCCR1A = 0x00;                // Normal mode, just as a Timer
  TCCR1B |= _BV(CS12);          // prescaler = CPU clock/1024
  TCCR1B &= ~_BV(CS11);      
  TCCR1B |= _BV(CS10);   
  TIMSK1 |= _BV(TOIE1);         // enable timer overflow interrupt
  TCNT1 = -15625;               // Ticks for 1 second @16 MHz,prescale=1024
}

ISR (TIMER1_OVF_vect)  //overflow時執行
{
  int percentage;
  float volts;
  if (!senddata)
  {
    senddata =1;
     num++;
    Serial.println(num);
  
    if(num >timeinterval)
    {
      Ethernet.begin(mac);
      num=0;

      sensors.requestTemperatures();
      temp=sensors.getTempCByIndex(0);

      X0 = getData(0x32); // 取得 X 軸 低位元資料
      X1 = getData(0x33); // 取得 X 軸 高位元資料
      X = ((X1 << 8)  + X0) / 256.0;
   
      Y0 = getData(0x34); // 取得 Y 軸 低位元資料
      Y1 = getData(0x35); // 取得 Y 軸 高位元資料
      Y = ((Y1 << 8)  + Y0) / 256.0;
   
      Z0 = getData(0x36); // 取得 Z 軸 低位元資料
      Z1 = getData(0x37); // 取得 Y 軸 高位元資料
      Z = ((Z1 << 8)  + Z0) / 256.0;
      Serial.print("temp");
      Serial.println(temp);
      Serial.print("X= ");
      Serial.print(X);
      Serial.print("    Y= ");
      Serial.print(Y);
      Serial.print("    Z= ");
      Serial.println(Z);
  
//      volts = MGRead(MG_PIN);
//      
//      percentage = MGGetPercentage(volts,CO2Curve);
//      Serial.print("CO2:");
//      if (percentage == -1) {
//          Serial.print( "<400" );
//      } else {
//          Serial.print(percentage);
//      }
//      Serial.println( "ppm" );  
      
      int sensorValue = analogRead(MG9_PIN);
      Serial.println(sensorValue);
      
      Serial.println( "=====================================================" ); 
      if(!getPagesensor(serverName,serverPort,pageNameg,(String)X+"/"+(String)Y+"/"+(String)Z))
        Serial.print(F("Fail "));
      else
        Serial.print(F("Pass "));
  
      if(!getPagesensor(serverName,serverPort,pageNametemp,(String)temp))
        Serial.print(F("Fail "));
      else
        Serial.print(F("Pass "));
  
      if(!getPagesensor(serverName,serverPort,pageNamegas,(String)sensorValue))
        Serial.print(F("Fail "));
      else
        Serial.print(F("Pass "));
      
      Serial.println( "=====================================================" );

      totalCount++;
      Serial.println(totalCount,DEC);
    }
    senddata=0;
  }
  TCNT1 = -15625;
}

String readmySerial()
{
  String readline="";
  
  while(Serial1.available())
  {
    senddata =1;
    char inChar = Serial1.read();
    if(inChar==(0x0D) |inChar==(0x0A)){
      break;
    }
    readline=readline+String(inChar);
    delay(5);
  }
  if (readline!="" && readline.charAt(1)=='/')
  {
      Serial.println(readline);
      if(!getPagesensor(serverName,serverPort,pageNamenfc,readline))
        Serial.print(F("Fail "));
      else
        Serial.print(F("Pass "));
    return readline;
  }
  else if (readline=="0")
  {
    Serial.println(readline);
    if(!getPagesensor(serverName,serverPort,pageNamelockset,readline))
      Serial.print(F("Fail "));
    else
      Serial.print(F("Pass "));
  }
  senddata =0;
//  return 0;
}

byte getPagesensor(char* domainBuffer,int thisPort,char* page,String thisData)
{
  int inChar;
  char outBuf[64];
  String cmd2 = "";
  String response="";
  Serial.print(F("connecting..."));

  if(client.connect(domainBuffer,thisPort) == 1)
  {
    Serial.println(F("connected"));
    
    cmd2 = cmd2+thisData;

    client.print("GET ");
    client.print(page);
    client.print(thisData);
    client.println(" HTTP/1.1 ");
    client.print("Host: ");
    client.println(domainBuffer);
    client.println("Connection: close\r\n");
  } 
  else
  {
    Serial.println(F("failed"));
    if (page==pageNamenfc)
    {
      Serial.println("response nfc");
      Serial1.println("failed");
    }
    return 0;
  }

  int connectLoop = 0;

  while(client.connected())
  {
    while(client.available())
    {
      inChar = client.read();
      response +=(char)inChar;
      if(response.endsWith("expires")| response.endsWith("http")|response.endsWith("\r\n\r\n")|response.endsWith("Set-Cookie:")|response.endsWith("laravel_session")|response.endsWith("Connection: close"))
      {
        response="";
      }
      connectLoop = 0;
    }

    delay(1);
    connectLoop++;
    if(connectLoop > 30000)
    {
      Serial.println();
      Serial.println(F("Timeout"));
      client.stop();
    }
  }

  Serial.println();
  Serial.println(F("disconnecting."));
  Serial.println(response);
  if (page==pageNamenfc)
  {
    Serial.println("response nfc");
    Serial1.println(response);
  }
  
  if (response.indexOf("1")>0)
  {
    digitalWrite(8,!digitalRead(8));
  }
  client.stop();
  return 1;
}

void setReg(int reg, int data){
    Wire.beginTransmission(I2C_Address);
    Wire.write(reg); // 指定佔存器
    Wire.write(data); // 寫入資料
    Wire.endTransmission();
}

/* getData(reg)：取得佔存器裡的資料
 * 參數：reg → 佔存器位址
 */
int getData(int reg){
    Wire.beginTransmission(I2C_Address);
    Wire.write(reg);
    Wire.endTransmission();
    
    Wire.requestFrom(I2C_Address,1);
    
    if(Wire.available()<=1){
        return Wire.read();
    }
}

//int  MGGetPercentage(float volts, float *pcurve)
//{
//   if ((volts/DC_GAIN )>=ZERO_POINT_VOLTAGE) {
//      return -1;
//   } else { 
//      return pow(10, ((volts/DC_GAIN)-pcurve[1])/pcurve[2]+pcurve[0]);
//   }
//}
//
//float MGRead(int mg_pin)
//{
//    int i;
//    float v=0;
//
//    for (i=0;i<5;i++) {
//        v += analogRead(mg_pin);
//        delay(50);
//    }
//    v = (v/5) *5/1024 ;
//    return v;  
//}



/*byte postPage(char* domainBuffer,int thisPort,char* page,String thisData)
{
  int inChar;
  char outBuf[64];
  String cmd2 = "value=";
  String response="";
  Serial.print(F("connecting..."));

  if(client.connect(domainBuffer,thisPort) == 1)
  {
    Serial.println(F("connected"));
    
    cmd2 = cmd2+thisData;
   
    client.print("GET ");
    client.print(pageName);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(domainBuffer);
    client.println("Connection: close\r\n");
    
//    client.print("POST ");
//    client.print(pageName);
//    client.println(" HTTP/1.1 ");
//    client.print("Host: ");
//    client.println(domainBuffer);
//    client.println("Content-Type: application/x-www-form-urlencoded");
//    client.println("Content-Length: "+(String )cmd2.length());
//    client.println();
//    client.println(cmd2);
  
  } 
  else
  {
    Serial.println(F("failed"));
    return 0;
  }

  int connectLoop = 0;

  while(client.connected())
  {
    while(client.available())
    {
      inChar = client.read();
//      Serial.write(inChar);
      response +=(char)inChar;
      connectLoop = 0;
    }

    delay(1);
    connectLoop++;
    if(connectLoop > 10000)
    {
      Serial.println();
      Serial.println(F("Timeout"));
      client.stop();
    }
  }

  Serial.println();
  Serial.println(F("disconnecting."));
  Serial.println(response);
//  Serial.println(response.indexOf("123"));
  if (response.indexOf("1")>0)
  {
    Serial.println("find 123");
    digitalWrite(8,!digitalRead(8));
  }
  client.stop();
//  digitalWrite(10,HIGH);   //關閉Ethernet
  return 1;
}*/



