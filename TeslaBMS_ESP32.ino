#include <Arduino.h>
#include "Logger.h"
#include "SerialConsole.h"
#include "BMSModuleManager.h"
#include "SystemIO.h"
#include <esp32_can.h>

#define BMS_BAUD  612500
//#define BMS_BAUD  617647
//#define BMS_BAUD  608695

//HardwareSerial Serial2(1); //define second serial port for Tesla BMS board

BMSModuleManager bms;
EEPROMSettings settings;
SerialConsole console;
uint32_t lastUpdate;

void loadSettings()
{
        Logger::console("Resetting to factory defaults");
        settings.version = EEPROM_VERSION;
        settings.checksum = 0;
        settings.canSpeed = 500000;
        settings.batteryID = 0x01; //in the future should be 0xFF to force it to ask for an address
        settings.OverVSetpoint = 4.1f;
        settings.UnderVSetpoint = 2.3f;
        settings.OverTSetpoint = 65.0f;
        settings.UnderTSetpoint = -10.0f;
        settings.balanceVoltage = 3.9f;
        settings.balanceHyst = 0.04f;
        settings.logLevel = 2;
        
    Logger::setLoglevel((Logger::LogLevel)settings.logLevel);
}

void initializeCAN()
{
    uint32_t id;
    CAN0.begin(settings.canSpeed);
    if (settings.batteryID < 0xF)
    {
        //Setup filter for direct access to our registered battery ID
        id = (0xBAul << 20) + (((uint32_t)settings.batteryID & 0xF) << 16);
        CAN0.setRXFilter(0, id, 0x1FFF0000ul, true);
        //Setup filter for request for all batteries to give summary data
        id = (0xBAul << 20) + (0xFul << 16);
        CAN0.setRXFilter(1, id, 0x1FFF0000ul, true);
    }
}

void setup() 
{
    delay(2000);  //just for easy debugging. It takes a few seconds for USB to come up properly on most OS's
    SERIALCONSOLE.begin(115200);
    SERIALCONSOLE.println("Starting up!");
    SERIAL.begin(BMS_BAUD, SERIAL_8N1, 4, 2); //rx pin is 4, tx pin is 2

    SERIALCONSOLE.println("Started serial interface to BMS.");

    pinMode(13, INPUT);

    Serial.println("Load EEPROM");
    loadSettings();
    Serial.println("Initialize CAN");
    initializeCAN();
    Serial.println("System IO setup");
    //systemIO.setup();

    Serial.println("Init BMS board numbers");
    bms.renumberBoardIDs();

    //Logger::setLoglevel(Logger::Debug);

    lastUpdate = 0;

    Serial.println("BMS clear faults");
    bms.clearFaults();
    Serial.println("End of setup");
    Serial.println("Send ? line to get help. d to get detailed updates, p to get summary updates.");
    delay(1000);
}

void loop() 
{
    CAN_FRAME incoming;

    console.loop();

    if (millis() > (lastUpdate + 1000))
    {    
        lastUpdate = millis();
        bms.balanceCells();
        bms.getAllVoltTemp();
    }

    if (CAN0.available()) {
        CAN0.read(incoming);
        bms.processCANMsg(incoming);
    }
}
