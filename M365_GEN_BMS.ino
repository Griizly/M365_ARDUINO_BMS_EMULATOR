#include <Arduino.h>
#include <EEPROM.h>
#include "PinChangeInterrupt.h"

#include <avr/sleep.h>
#include <avr/power.h>

//#include "bq769x0.h"
#include "main.h"

M365BMS g_M365BMS;
BMSSettings g_Settings;

bool g_Debug = false;
// I2CAddress = 0x08, crcEnabled = true
//bq769x0 g_BMS(bq76940, 0x08, true);
volatile bool g_interruptFlag = false;
unsigned long g_lastActivity = 0;
unsigned long g_lastUpdate = 0;

void alertISR()
{
    //g_BMS.setAlertInterruptFlag();
    g_interruptFlag = true;
}

void setup()
{
    // 115200 baud fix
    // https://forum.arduino.cc/index.php?topic=54623.0
    // http://codebender.cc/sketch:186647
    //code for arduino pro mini
    //OSCCAL = 163;
    delay(100);

    Serial.begin(115200);
    Serial.println(F("BOOTED!"));
    

    power_adc_disable();
    power_spi_disable();
    power_timer1_disable();
    power_timer2_disable();
    power_twi_disable();
    delay(100);
//Serial.println(F("1"));
    // Disable TX
    UCSR0B &= ~(1 << TXEN0);

    loadSettings();
//Serial.println(F("2"));
    //g_BMS.begin(BMS_BOOT_PIN);

    applySettings();
//Serial.println(F("3"));
    // attach ALERT interrupt
    pinMode(BMS_ALERT_PIN, INPUT);
    attachPCINT(digitalPinToPCINT(BMS_ALERT_PIN), alertISR, RISING);
    interrupts();

    delay(250);
    //g_BMS.update();
    //g_BMS.resetSOC(100);

    //g_BMS.enableDischarging();
    //g_BMS.enableCharging();
    
}


void loadSettings()
{
    if(EEPROM.read(0) != g_Settings.header[0] || EEPROM.read(1) != g_Settings.header[1])
    {
        for(uint16_t i = 0 ; i < EEPROM.length() ; i++) {
            EEPROM.write(i, 0);
        }
        EEPROM.put(0, g_Settings);
    }
    else
    {
        EEPROM.get(0, g_Settings);
    }
}

void saveSettings()
{
    EEPROM.put(0, g_Settings);
}

void applySettings()
{
    /*g_BMS.setBatteryCapacity(g_Settings.capacity, g_Settings.nominal_voltage, g_Settings.full_voltage);

    g_BMS.setShuntResistorValue(g_Settings.shuntResistor_uOhm);
    g_BMS.setThermistorBetaValue(g_Settings.thermistor_BetaK);

    g_BMS.setTemperatureLimits(g_Settings.temp_minDischargeC,
                             g_Settings.temp_maxDischargeC,
                             g_Settings.temp_minChargeC,
                             g_Settings.temp_maxChargeC);
    g_BMS.setShortCircuitProtection(g_Settings.SCD_current, g_Settings.SCD_delay);
    g_BMS.setOvercurrentChargeProtection(g_Settings.OCD_current, g_Settings.OCD_delay);
    g_BMS.setOvercurrentDischargeProtection(g_Settings.ODP_current, g_Settings.ODP_delay);
    g_BMS.setCellUndervoltageProtection(g_Settings.UVP_voltage, g_Settings.UVP_delay);
    g_BMS.setCellOvervoltageProtection(g_Settings.OVP_voltage, g_Settings.OVP_delay);

    g_BMS.setBalancingThresholds(g_Settings.balance_minIdleTime,
                               g_Settings.balance_minVoltage,
                               g_Settings.balance_maxVoltageDiff);
    g_BMS.setIdleCurrentThreshold(g_Settings.idle_currentThres);
    if(g_Settings.balance_enabled)
        g_BMS.enableAutoBalancing();
    else
        g_BMS.disableAutoBalancing();

    g_BMS.adjADCPackOffset(g_Settings.adcPackOffset);
    g_BMS.adjADCCellsOffset(g_Settings.adcCellsOffset);*/

    strncpy(g_M365BMS.serial, g_Settings.serial, sizeof(g_M365BMS.serial));
    g_M365BMS.design_capacity = g_M365BMS.unk_capacity = g_Settings.capacity;
    g_M365BMS.nominal_voltage = g_Settings.nominal_voltage;
    g_M365BMS.date = g_Settings.date;
    g_M365BMS.num_cycles = g_Settings.num_cycles;
    g_M365BMS.num_charged = g_Settings.num_charged;
}


void onNinebotMessage(NinebotMessage &msg)
{
    g_lastActivity = millis();
    if(!(UCSR0B & (1 << TXEN0))) {
        // Enable TX
        UCSR0B |= (1 << TXEN0);
    }

    if(msg.addr != M365BMS_RADDR)
        return;

    if(msg.mode == 0x01 || msg.mode == 0xF1)
    {
        if(msg.length != 3)
            return;

        uint16_t ofs = (uint16_t)msg.offset * 2; // word aligned
        uint8_t sz = msg.data[0];

        if(sz > sizeof(NinebotMessage::data))
            return;

        msg.addr = M365BMS_WADDR;
        msg.length = 2 + sz;

        if(msg.mode == 0x01)
        {
            if((ofs + sz) > sizeof(g_M365BMS))
                return;

            memcpy(&msg.data, &((uint8_t *)&g_M365BMS)[ofs], sz);
        }
        else if(msg.mode == 0xF1)
        {
            if((ofs + sz) > sizeof(g_Settings))
                return;

            memcpy(&msg.data, &((uint8_t *)&g_Settings)[ofs], sz);
        }

        ninebotSend(msg);
    }
    else if(msg.mode == 0x03 || msg.mode == 0xF3)
    {
        uint16_t ofs = (uint16_t)msg.offset * 2; // word aligned
        uint8_t sz = msg.length - 2;

        if(msg.mode == 0x03)
        {
            if((ofs + sz) > sizeof(g_M365BMS))
                return;

            memcpy(&((uint8_t *)&g_M365BMS)[ofs], &msg.data, sz);
        }
        else if(msg.mode == 0xF3)
        {
            if((ofs + sz) > sizeof(g_Settings))
                return;

            memcpy(&((uint8_t *)&g_Settings)[ofs], &msg.data, sz);
        }
    }
    else if(msg.mode == 0xFA)
    {
        switch(msg.offset)
        {
            case 1: {
                applySettings();
            } break;
            case 2: {
                EEPROM.get(0, g_Settings);
            } break;
            case 3: {
                EEPROM.put(0, g_Settings);
            } break;
//#if BQ769X0_DEBUG
            case 4: {
                g_Debug = msg.data[0];
            } break;
            case 5: {
                debug_print();
            } break;
//#endif
            case 6: {
                /*g_BMS.disableDischarging();
                g_BMS.disableCharging();*/
            } break;
            case 7: {
                /*g_BMS.enableDischarging();
                g_BMS.enableCharging();*/
            } break;
        }
    }
}

void ninebotSend(NinebotMessage &msg)
{
    msg.checksum = (uint16_t)msg.length + msg.addr + msg.mode + msg.offset;

    Serial.write(msg.header[0]);
    Serial.write(msg.header[1]);
    Serial.write(msg.length);
    Serial.write(msg.addr);
    Serial.write(msg.mode);
    Serial.write(msg.offset);
    for(uint8_t i = 0; i < msg.length - 2; i++)
    {
        Serial.write(msg.data[i]);
        msg.checksum += msg.data[i];
    }

    msg.checksum ^= 0xFFFF;
    Serial.write(msg.checksum & 0xFF);
    Serial.write((msg.checksum >> 8) & 0xFF);
}

void ninebotRecv()
{
    static NinebotMessage msg;
    static uint8_t recvd = 0;
    static unsigned long begin = 0;
    static uint16_t checksum;

    while(Serial.available())
    {
        if(millis() >= begin + 10)
        { // 10ms timeout
            recvd = 0;
        }

        uint8_t byte = Serial.read();
        recvd++;

        switch(recvd)
        {
            case 1:
            {
                if(byte != 0x55)
                { // header1 mismatch
                    recvd = 0;
                    break;
                }

                msg.header[0] = byte;
                begin = millis();
            } break;

            case 2:
            {
                if(byte != 0xAA)
                { // header2 mismatch
                    recvd = 0;
                    break;
                }

                msg.header[1] = byte;
            } break;

            case 3: // length
            {
                if(byte < 2)
                { // too small
                    recvd = 0;
                    break;
                }

                msg.length = byte;
                checksum = byte;
            } break;

            case 4: // addr
            {
                if(byte != M365BMS_RADDR)
                { // we're not the receiver of this message
                    recvd = 0;
                    break;
                }

                msg.addr = byte;
                checksum += byte;
            } break;

            case 5: // mode
            {
                msg.mode = byte;
                checksum += byte;
            } break;

            case 6: // offset
            {
                msg.offset = byte;
                checksum += byte;
            } break;

            default:
            {
                if(recvd - 7 < msg.length - 2)
                { // data
                    msg.data[recvd - 7] = byte;
                    checksum += byte;
                }
                else if(recvd - 7 - msg.length + 2 == 0)
                { // checksum LSB
                    msg.checksum = byte;
                }
                else
                { // checksum MSB and transmission finished
                    msg.checksum |= (uint16_t)byte << 8;
                    checksum ^= 0xFFFF;

                    if(checksum != msg.checksum)
                    { // invalid checksum
                        recvd = 0;
                        break;
                    }

                    onNinebotMessage(msg);
                    recvd = 0;
                }
            } break;
        }
    }
}

//#if BQ769X0_DEBUG

//#endif

void loop()
{
  
    unsigned long now = millis();
    if(g_interruptFlag || (unsigned long)(now - g_lastUpdate) >= 1000)
    {
        if(g_interruptFlag)
            g_interruptFlag = false;

        //g_BMS.update(); // should be called at least every 250 ms
        g_lastUpdate = now;

        // update M365BMS struct
        {
            // charging state
            if(550 > (int16_t)g_Settings.idle_currentThres)/*g_BMS.getBatteryCurrent()*/
                g_M365BMS.status |= 64;
            else if(450 < (int16_t)g_Settings.idle_currentThres / 2)/*g_BMS.getBatteryCurrent()*/
                g_M365BMS.status &= ~64;

            uint16_t batVoltage = 38200 / 10;
            //if(batVoltage > g_M365BMS.max_voltage)
                g_M365BMS.max_voltage = batVoltage;

            int16_t batCurrent = 1700;/*g_BMS.getBatteryCurrent()*/
            //if(batCurrent > 0 && (uint16_t)batCurrent > g_M365BMS.max_charge_current)
                g_M365BMS.max_charge_current = batCurrent;
            //else if(batCurrent < 0 && (uint16_t)-batCurrent > g_M365BMS.max_discharge_current)
                g_M365BMS.max_discharge_current = batCurrent;/*-batCurrent*/

            g_M365BMS.capacity_left =  7800;/*g_M365BMS.design_capacity*/
            g_M365BMS.percent_left = 100.00;
            g_M365BMS.current = batCurrent;
            g_M365BMS.voltage = batVoltage;
            g_M365BMS.temperature[0] = 25.00 +20.00;
            g_M365BMS.temperature[1] = 25.00 +20.00;

            /*if(g_BMS.batCycles_) {
                g_BMS.batCycles_ = 0;*/
                g_M365BMS.num_cycles = 25;
                EEPROM.put(0, g_Settings);
            //}

            /*if(g_BMS.chargedTimes_) {
                g_BMS.chargedTimes_ = 0;*/
                g_M365BMS.num_charged = 15;
            //}

            byte numCells = 15;
            for(byte i = 0; i < numCells; i++)
                g_M365BMS.cell_voltages[i] = 3820;
        }
        //Serial.println(F("Loop 1"));
    }

    ninebotRecv();

    //Serial.println(F("Loop 2"));
    if((unsigned long)(now - g_lastActivity) >= 1000) {
        if(!g_Debug && UCSR0B & (1 << TXEN0)) {
            // Disable TX
            UCSR0B &= ~(1 << TXEN0);
            Serial.println(F("Loop 3"));
        }
    }
}

void debug_print()
{
    //g_BMS.printRegisters();
    Serial.println(F(""));

    Serial.print(F("Battery voltage: "));
    Serial.print(g_M365BMS.voltage);
    Serial.print(F(" ("));
    Serial.print(g_M365BMS.voltage);
    Serial.println(F(")"));

    Serial.print(F("Battery current: "));
    Serial.print(g_M365BMS.current);
    Serial.print(F(" ("));
    Serial.print(g_M365BMS.current);
    Serial.println(F(")"));

    Serial.println(F("SOC: "));
    //Serial.println(g_BMS.getSOC());

    Serial.print(F("Temperature: "));
    Serial.print(g_M365BMS.temperature[0]);
    Serial.print(F(" "));
    Serial.println(g_M365BMS.temperature[1]);

    Serial.print(F("Balancing status: "));
    //Serial.println(g_BMS.getBalancingStatus());

    Serial.print(F("Cell voltages ("));
    int numCells = 15;
    Serial.print(numCells);
    Serial.println(F("):"));
    for(int i = 0; i < numCells; i++) {
        Serial.print(g_M365BMS.cell_voltages[i]);
        Serial.print(F(" ("));
        Serial.print(g_M365BMS.cell_voltages[i]);
        Serial.print(F(")"));
        if(i != numCells - 1)
            Serial.print(F(", "));
    }
    Serial.println(F(""));

    /*Serial.print(F("Cell V: Min: "));
    Serial.print(g_BMS.getMinCellVoltage());
    Serial.print(F(" | Avg: "));
    Serial.print(g_BMS.getAvgCellVoltage());
    Serial.print(F(" | Max: "));
    Serial.print(g_BMS.getMaxCellVoltage());
    Serial.print(F(" | Delta: "));
    Serial.println(g_BMS.getMaxCellVoltage() - g_BMS.getMinCellVoltage());*/
    Serial.print(F("maxVoltage: "));
    Serial.println(g_M365BMS.max_voltage);
    Serial.print(F("maxDischargeCurrent: "));
    Serial.println(g_M365BMS.max_discharge_current);
    Serial.print(F("maxChargeCurrent: "));
    Serial.println(g_M365BMS.max_charge_current);
}
