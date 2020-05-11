#include <SPI.h>
#include "VanMessageSender.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <avr/wdt.h>

const int VAN_CS_PIN = 10;

AbstractVanMessageSender *VANInterface;

SPIClass* spi;
uint8_t headerByte = 0x80;

char incomingByte;

//State Variables
bool eeprom_data = false;
bool alarm = false;
bool silence = false;
bool buzzing = false;

//Flags
bool door_opening = false;
bool door_open = false;
bool door_closing = false;
bool remote_lock = false;
bool remote_unlock = false;
bool remote_unlock_tmp = false;
bool remote_lock_fail = false;
bool remote_lock_invalid = false;
bool go_silent = false;
bool go_noisy = false;
bool warning_noise = false;
bool should_send = false;

//Variables
unsigned long remote_unlock_check = millis();
unsigned long lastSent = millis();
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
  
  EEPROM.get(42,eeprom_data);
  if (eeprom_data == true){
    buzzing = true;
    alarm = true;
  }
  VANInterface->reset_channels();
  if(!alarm_setup()) failure();
}

void softwareReset( uint8_t prescaller) {
  // start watchdog with the provided prescaller
  wdt_enable( prescaller);
  // wait for the prescaller time to expire
  // without sending the reset signal by using
  // the wdt_reset() method
  while(1) {}
}

bool alarm_setup(){
  if(!VANInterface->set_channel_for_receive_message(0, 0x55A, 2, 1)){
        Serial.println("ERRO na transmissão");
        return false;
  }
  if(!VANInterface->set_channel_for_receive_message(1, 0x55A, 2, 1)){
        Serial.println("ERRO na transmissão");  
        return false;
  }
  if(!VANInterface->set_channel_for_receive_message(2, 0x45A, 1, 1)){
        Serial.println("ERRO na transmissão");
        return false;
  }
  if(!VANInterface->set_channel_for_receive_message(3, 0x45A, 1, 1)){
        Serial.println("ERRO na transmissão");  
        return false;
  }
  return true;
}

void alarm_monitor(){
  String t;
  uint8_t vanMessageLength;
  uint8_t vanMessage[32];
  for (uint8_t channel = 0; channel < 4; channel++){
    MessageLengthAndStatusRegister messageAvailable = VANInterface->message_available(channel);
    if (messageAvailable.data.CHRx /*|| messageAvailable.data.CHTx || messageAvailable.data.CHER*/){
      VANInterface->read_message(channel, &vanMessageLength, vanMessage);
      if (vanMessage[0] == 0x00){
        return;
      }
      char buff[4];
      for (size_t i = 0; i < vanMessageLength; i++){
        snprintf(buff, 4, "%02X ", vanMessage[i]);
        t += String(buff);
      }
//      Serial.println(t);
      
      if(t.indexOf("55 A9 C0")>=0){
        if(alarm){
          remote_unlock = true; 
        }else{
          remote_unlock_tmp = true;
          remote_unlock_check = millis();
        }
      } 
      if(t.indexOf("55 A9 00 80")>=0){//it was ignition that sent remote unlock
        remote_unlock_tmp = false;
      }
      if(t.indexOf("55 A9 40")>=0){
        remote_lock = true;
      }
      if(t.indexOf("55 A9 10")>=0){
        remote_lock_fail = true;
      }
//      if(t.indexOf("55 A9 00 00")>=0){
//        //Unknown
//      }
      if(t.indexOf("45 A9 E0")>=0){
        door_opening = true;
      }
      if(t.indexOf("45 A9 A0")>=0){
        door_closing = true;
      }
      if(t.indexOf("45 A9 60")>=0){
        door_open = true;
      }    
      if(t.indexOf("45 A9 20")>=0){ //door closed
        door_open = false;
      }
      t = "";
      VANInterface->reactivate_channel(channel);
    }
  }
}

void lock(){
  if(!silence)
    Serial.print("lock\n");
}

void unlock(){
  if(!silence)
    Serial.print("unlock\n");
}

void beep(){
  Serial.print("beep\n");
}

void un_beep(){
  Serial.print("un_beep\n");
}

void un_beep_warning(){
  Serial.print("un_beep_war\n");
}

void beep_going_silent(){
  Serial.print("beep_slt\n");
}

void beep_going_noisy(){
  Serial.print("beep_nsy\n");
}

void failure(){
  Serial.print("failure\n");
  // restart in 60 milliseconds
  softwareReset( WDTO_60MS);
}

void fail_to_lock(){
  Serial.print("failure\n");
}

void logic(){
  //Coherence
  if(remote_lock && remote_unlock){
    remote_lock == false;
    remote_unlock == false;
  }
  //Start beeping
  if(door_opening || door_open || door_closing){
    if(alarm){
      buzzing = true;
      should_send = true;
    }
    door_opening = false;
    door_open = false;
    door_closing = false;
  }
  //Locking with door opened
  if(remote_lock_fail){
    remote_lock_invalid = true;
    should_send = true;
  }
  //Turning on ignition sends remote unlock, but should not go to silence
  if(remote_unlock_tmp && (millis()-remote_unlock_check)>1100){
    remote_unlock = true;
    remote_unlock_tmp = false;
  }
  //Silent mode
  if(remote_unlock && !alarm){
    if(!silence)
      go_silent = true;
    else 
      go_noisy = true;
    remote_unlock = false;
    should_send = true;
  }
  //locking
  if(remote_lock){
    if(!remote_lock_invalid){
      alarm = true;
      should_send = true;
    }else{
      remote_lock_invalid = false;
      remote_lock = false;
    }
  }
  //unlocking
  if(remote_unlock){
    alarm = false;
    buzzing = false;
    should_send = true;
  }
}

void loop() {
  alarm_monitor();
  logic();
  currentTime = millis();
  if((currentTime - lastSent) > 1000){
    should_send = true;
    lastSent = currentTime;
  }    
  if(should_send){
    should_send = false;
    if(buzzing) {
      EEPROM.get(42,eeprom_data);
      if(eeprom_data == false)
        EEPROM.put(42, true);
      beep();
    }
    else{
      EEPROM.get(42,eeprom_data);
      if(eeprom_data == true)
        EEPROM.put(42, false);
      un_beep();
    }
    if(warning_noise){
      warning_noise = false;
      un_beep_warning();
    }
    if(remote_lock){
      remote_lock = false;
      lock();
    }
    if(remote_unlock){
      remote_unlock = false;
      unlock();
    }
    if(remote_lock_fail){
      remote_lock_fail = false;
      fail_to_lock();
    }    
    if(go_silent){
      go_silent = false;
      silence = true;
      beep_going_silent();
    }
    if(go_noisy){
      go_noisy = false;
      silence = false;
      beep_going_noisy();
    }
  }
}
