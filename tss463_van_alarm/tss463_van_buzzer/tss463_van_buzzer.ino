#include <SPI.h>
#include "VanMessageSender.h"
#include <Arduino.h>
#include <EEPROM.h>

const int SCK_PIN = 13;
const int MISO_PIN = 12;
const int MOSI_PIN = 11;
const int VAN_CS_PIN = 10;
const int RELAY_PIN = 8;

AbstractVanMessageSender *VANInterface;

SPIClass* spi;
int count = 0;
uint8_t headerByte = 0x80;

bool WAIT = false;
bool flip = true;
bool bulb = true;
bool buzzer = false;
bool set_channel = false;
bool eeprom_data = false;
unsigned long currentTime = millis();
unsigned long previousTime = millis();

void setup(){
  Serial.begin(230400);
  Serial.println("Arduino VAN bus monitor using TSS463C");
  // initialize SPI
  spi = new SPIClass();
  spi->begin();

  // instantiate the VanMessageSender class passing the CS pin and the SPI instance as a dependency
  VANInterface = new VanMessageSender(VAN_CS_PIN, spi);
  VANInterface->begin();
  
  pinMode(RELAY_PIN, OUTPUT);    
  digitalWrite(RELAY_PIN,LOW);

  EEPROM.get(24,eeprom_data);
  if (eeprom_data == true){
    buzzer = true;
  }
}

void setup_channel(){
  headerByte = (headerByte == 0x87) ? 0x80 : headerByte + 1;
  uint8_t packet1[10] = { headerByte, 0x20, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, headerByte};
  if(!VANInterface->set_channel_for_immediate_reply_message(0, 0x450, packet1, 10))
    Serial.println("ERRO na transmissão");
  if(!VANInterface->set_channel_for_reply_request_message(1, 0x450, 10, 1))
    Serial.println("ERRO na transmissão");
  set_channel = true;
}

void beginning(){
  headerByte = (headerByte == 0x87) ? 0x80 : headerByte + 1;
  digitalWrite(RELAY_PIN, HIGH);
  delay(10);
  if(!set_channel) 
    setup_channel();
  VANInterface->set_value_in_channel(0, 0, headerByte);
  VANInterface->set_value_in_channel(0, 1, 0x20);
  VANInterface->set_value_in_channel(0, 7, 0x00);
  VANInterface->set_value_in_channel(0, 9, headerByte);
  send_frame(3);
}

void light(int duration = 5){
  headerByte = (headerByte == 0x87) ? 0x80 : headerByte + 1;
  if(!set_channel) 
    setup_channel();
  VANInterface->set_value_in_channel(0, 0, headerByte);
  VANInterface->set_value_in_channel(0, 1, 0x28);
  VANInterface->set_value_in_channel(0, 9, headerByte);
  send_frame(duration);
  VANInterface->set_value_in_channel(0, 1, 0x20);
}

void beep(int duration = 5){
  headerByte = (headerByte == 0x87) ? 0x80 : headerByte + 1;
  if(!set_channel) 
    setup_channel();
  VANInterface->set_value_in_channel(0, 0, headerByte);
  VANInterface->set_value_in_channel(0, 7, 0x80);
  VANInterface->set_value_in_channel(0, 9, headerByte);
  send_frame(duration);  
  VANInterface->set_value_in_channel(0, 7, 0x00);
}

void light_beep(int duration = 5){
  headerByte = (headerByte == 0x87) ? 0x80 : headerByte + 1;
  if(!set_channel) 
    setup_channel();
  VANInterface->set_value_in_channel(0, 0, headerByte);
  VANInterface->set_value_in_channel(0, 1, 0x28);
  VANInterface->set_value_in_channel(0, 7, 0x80);
  VANInterface->set_value_in_channel(0, 9, headerByte);
  send_frame(duration);  
  VANInterface->set_value_in_channel(0, 1, 0x20);
  VANInterface->set_value_in_channel(0, 7, 0x00);
}

void ending(){
  headerByte = (headerByte == 0x87) ? 0x80 : headerByte + 1;
  VANInterface->set_value_in_channel(0, 0, headerByte);
  VANInterface->set_value_in_channel(0, 9, headerByte);
  send_frame(3);
  delay(10);
  digitalWrite(RELAY_PIN, LOW);
}

bool pre_check(int duration = 2){
  int start = millis();
  int current = start;
  int i = 0;
  String t;
  uint8_t vanMessageLength;
  uint8_t vanMessage[32];
  if(!set_channel) 
    setup_channel();
  
  while(i<duration){
    current = millis();
    start = current;
    while(!VANInterface->reactivate_channel(1) && ((current - start) < 50)){
      current = millis();
      Serial.println("delayed");
      delay(1);
    }
    
//    Serial.println(millis());
    current = millis();
    start = current;
    while(((current - start) < 50)){
      current = millis();
      MessageLengthAndStatusRegister messageAvailable = VANInterface->message_available(1);
//      Serial.println(messageAvailable.Value, BIN);
      if (messageAvailable.data.CHRx || messageAvailable.data.CHTx || messageAvailable.data.CHER){
        if(messageAvailable.data.CHTx){
          VANInterface->read_message(1, &vanMessageLength, vanMessage);
          char buff[4];
          for (size_t i = 0; i < vanMessageLength; i++){
            snprintf(buff, 4, "%02X ", vanMessage[i]);
            
            t += String(buff);
          }
//          Serial.println(millis());
//          Serial.println("Channel 1");
//          Serial.println(t);
          if(t.indexOf("45 0")>=0){
            if(vanMessage[10] == 0xC0 || vanMessage[10] == 0xA0){
//              Serial.println("Remote Signal");
              buzzer = false;
              return false;
            }
            if((vanMessage[10] == 0x00)){
              flip = false;
            }
            if((vanMessage[10] == 0x60 && flip == false)){
              flip = true;
              Serial.println("Signal");
              buzzer = false;
              return false;
            }
          }
//          Serial.println("tx succefully");
        }else{
//          Serial.println("tx failed");
          
        }
        t="";
        break;
      }
    }
//    delay(5);
    i++;
  }
  return true;
}

void send_frame(int duration){
  int start = millis();
  int current = start;
  int i = 0;
  while(i<duration){
//    Serial.println(millis());
    current = millis();
    start = current;
    while(!VANInterface->reactivate_channel(0) && ((current - start) < 50)){
      current = millis();
//      Serial.println("delayed");
      delay(1);
    }
//    Serial.println("wait for tx");
//    Serial.println(millis());
    current = millis();
    start = current;
    while(((current - start) < 50)){
      current = millis();
      MessageLengthAndStatusRegister messageAvailable = VANInterface->message_available(0);
//      Serial.println(messageAvailable.Value, BIN);
      if (messageAvailable.data.CHRx || messageAvailable.data.CHTx || messageAvailable.data.CHER){
        if(messageAvailable.data.CHTx){
//          Serial.println("tx succefully");
        }else{
//          Serial.println("tx failed");
          
        }
        break;
      }
    }
    i++;
//    Serial.println(millis());
  }
}

void light_once(){
  beginning();
  light(4);
  ending();
}

void beep_once(){
  beginning();
  beep(3);
  ending();
}

void reset_beep(){
  digitalWrite(RELAY_PIN, LOW);
  VANInterface->reset_channels();
  set_channel = false;
}

void loop() {
  String t;
  if(buzzer){
    EEPROM.get(24,eeprom_data);
    if(eeprom_data == false)
      EEPROM.put(24, true);
    currentTime = millis();
    if ((currentTime - previousTime) >= 500){
      if(!pre_check()) return;
      beginning();
      beep(6);
      ending();if(
      !pre_check()) return;
      delay(40);
      if(!pre_check()) return;
      beginning();
      light(4);
      ending();
      currentTime = millis();
      previousTime = currentTime;
    }
  }else{
    EEPROM.get(24,eeprom_data);
    if(eeprom_data == true)
      EEPROM.put(24, false);
  }
  if (Serial.available()>0){
    String in = Serial.readStringUntil('\n');
//        Serial.println(in);
    if(in == "lock"){
        beep_once();
        Serial.println("ack");
    }
    if(in == "unlock"){
        beep_once();
        delay(100);
        beep_once();
        Serial.println("ack");
    }
    if(in == "beep"){
        buzzer = true;
        Serial.println("ack");
    }
    if(in == "un_beep"){
        reset_beep();
        buzzer = false;
        Serial.println("ack");
    }
    if(in == "un_beep_war"){
        reset_beep();
        beep_once();
        delay(100);
        beep_once();
        delay(100);
        beep_once();
        buzzer = false;
        Serial.println("ack");
    }  
    if(in == "beep_slt"){
        light_once();
        delay(100);
        light_once();
        Serial.println("ack");
    }  
    if(in == "beep_nsy"){
        light_once();
        delay(100);
        beep_once();
        Serial.println("ack");
    }  
    if(in == "failure"){
        beep_once();
        light_once();
        delay(100);
        beep_once();
        Serial.println("ack");
    }  
  }
}
