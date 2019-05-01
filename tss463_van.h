// tss463_van.h

#ifndef _TSS463_VAN_h
#define _TSS463_VAN_h

#include "tss463_channel_registers_struct.h"

#if defined(ARDUINO) && ARDUINO >= 100
    #include <Arduino.h>
    #include <inttypes.h>
#else
    #include "WProgram.h"
#endif

#define TSS463_SELECT()   digitalWrite(SPICS, LOW)
#define TSS463_UNSELECT() digitalWrite(SPICS, HIGH)

#define MOTOROLA_MODE 0x00
#define WRITE         0xE0
#define READ          0x60
#define ADDR_ANSW     0xAA
#define CMD_ANSW      0x55

#define ROKE  (1)
#define RNOKE (0)


#pragma region TSS463C internal register adresses - Figure 22
                                 // R/W?  - Default value on init
                                 //------------------------------
#define LINECONTROL       0x00   // r/w   - 0x00
#define TRANSMITCONTROL   0x01   // r/w   - 0x02
#define DIAGNOSISCONTROL  0x02   // r/w   - 0x00
#define COMMANDREGISTER   0x03   // w     - 0x00
#define LINESTATUS        0x04   // r     - 0bx01xxx00
#define TRANSMITSTATUS    0x05   // r     - 0x00
#define LASTMESSAGESTATUS 0x06   // r     - 0x00
#define LASTERRORSTATUS   0x07   // r     - 0x00
#define INTERRUPTSTATUS   0x09   // r     - 0x80
#define INTERRUPTENABLE   0x0A   // r/w   - 0x80
#define INTERRUPTRESET    0x0B   // w
#pragma endregion
// Channels
#define CHANNEL_ADDR(x) (0x10 + (0x08 * x))
#define CHANNELS 14
// Mailbox - data register
#define GETMAIL(x) (0x80 + x)

class TSS463_VAN
{
private:
    volatile int error = 0; // TSS463C out of sync error
    uint8_t SPICS;
    void tss_init();
    void motorolla_mode();
    uint8_t spi_transfer(volatile uint8_t data);
    void register_set(uint8_t address, uint8_t value);
    uint8_t register_get(uint8_t address);
    uint8_t registers_get(const uint8_t address, volatile uint8_t values[], const uint8_t count);
    void registers_set(const uint8_t address, const uint8_t values[], const uint8_t n);
    void setup_channel(const uint8_t channelId, const uint8_t id1, const uint8_t id2, const uint8_t id2AndCommand, const uint8_t messagePointer, const uint8_t lengthAndStatus);
public:

    TSS463_VAN(uint8_t _CS);
    void set_channel_for_transmit_message(uint8_t channelId, uint8_t id1, uint8_t id2, const uint8_t values[], uint8_t messageLength, uint8_t ack);
    void set_channel_for_receive_message(uint8_t channelId, uint8_t id1, uint8_t id2, uint8_t messageLength);
    void set_channel_for_reply_request_message_without_transmission(uint8_t channelId, uint8_t id1, uint8_t id2, uint8_t messageLength);
    void set_channel_for_reply_request_message(uint8_t channelId, uint8_t id1, uint8_t id2, uint8_t messageLength);
    void set_channel_for_immediate_reply_message(uint8_t channelId, uint8_t id1, uint8_t id2, const uint8_t values[], uint8_t messageLength);
    void set_channel_for_deferred_reply_message(uint8_t channelId, uint8_t id1, uint8_t id2, const uint8_t values[], uint8_t messageLength);
    void set_channel_for_reply_request_detection_message(uint8_t channelId, uint8_t id1, uint8_t id2, uint8_t messageLength);
    void disable_channel(const uint8_t channelId);
    MessageLengthAndStatusRegister message_available(uint8_t channelId);
    uint8_t readMsgBuf(const uint8_t channelId, uint8_t*len, uint8_t buf[]);
    uint8_t getlastChannel();
    void begin();
};

extern TSS463_VAN VAN;

#endif