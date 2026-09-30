/*
  KP_24V test firmware for Arduino Nano

  Reads the battery voltage from A0, a DS18B20 temperature sensor on D7,
  and two PS2420G dry-contact states through PC817 optocouplers on D6/D5.
  Periodically sends scaled integer values as ASCII frames:

    V:<voltage in tenths of a volt>\r\n
    T:<temperature in tenths of a degree Celsius>\r\n
    C1:<UPS 1 alarm: 0 = normal, 1 = alarm/contact open>\r\n
    C2:<UPS 2 alarm: 0 = normal, 1 = alarm/contact open>\r\n

  Examples: 20.0 V is sent as V:200; 23.4 C is sent as T:234.

  Bench wiring:
    Nano D1/TX -> 1 kOhm -> main Arduino Uno D8/RX
    Nano GND   -> main Arduino Uno GND

    DS18B20 VCC  -> Nano +5V
    DS18B20 DATA -> Nano D7
    DS18B20 GND  -> Nano GND
    4.7 kOhm between DATA and +5V

  D0/RX is not required for this one-way test.
*/

#include <GyverDS18.h>

const byte ANALOG_INPUT_PIN = A0;
const byte DS18B20_PIN = 7;
const byte UPS1_ALARM_INPUT_PIN = 6;
const byte UPS2_ALARM_INPUT_PIN = 5;
const unsigned long SEND_INTERVAL_MS = 500UL;
const int16_t TEMPERATURE_ERROR_VALUE = 32767;

// Bench calibration with USB power and the assembled 30k + 30k / 10k divider.
const int ADC_CAL_LOW = 144;
const int ADC_CAL_HIGH = 880;
const int VOLTAGE_CAL_LOW_DV = 50;
const int VOLTAGE_CAL_HIGH_DV = 300;

GyverDS18Single temperatureSensor(DS18B20_PIN, false);

unsigned long lastSendTime = 0;

uint16_t adcToDecivolts(uint16_t adc)
{
  if (adc <= 3) {
    return 0;
  }

  const int32_t adcSpan = ADC_CAL_HIGH - ADC_CAL_LOW;
  const int32_t voltageSpan = VOLTAGE_CAL_HIGH_DV - VOLTAGE_CAL_LOW_DV;
  int32_t numerator = ((int32_t)adc - ADC_CAL_LOW) * voltageSpan;

  // Round to the nearest tenth for both positive and negative offsets.
  numerator += (numerator >= 0) ? adcSpan / 2 : -adcSpan / 2;

  int32_t decivolts = VOLTAGE_CAL_LOW_DV + numerator / adcSpan;
  if (decivolts < 0) {
    decivolts = 0;
  }
  if (decivolts > 65535L) {
    decivolts = 65535L;
  }
  return (uint16_t)decivolts;
}

int16_t temperatureToTenths(int16_t rawSixteenths)
{
  int32_t scaled = (int32_t)rawSixteenths * 10L;
  scaled += (scaled >= 0) ? 8 : -8;
  return (int16_t)(scaled / 16L);
}

void sendAlarmFrame(const __FlashStringHelper *name, byte pin)
{
  // Closed UPS contact energizes the PC817, so LOW is the normal state.
  const bool alarm = digitalRead(pin) == HIGH;
  Serial.print(name);
  Serial.println(alarm ? 1 : 0);
}

void setup()
{
  Serial.begin(9600);
  pinMode(UPS1_ALARM_INPUT_PIN, INPUT_PULLUP);
  pinMode(UPS2_ALARM_INPUT_PIN, INPUT_PULLUP);
  temperatureSensor.requestTemp();
}

void loop()
{
  unsigned long now = millis();

  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = now;

    uint16_t rawA0 = analogRead(ANALOG_INPUT_PIN);

    Serial.print(F("V:"));
    Serial.println(adcToDecivolts(rawA0));

    sendAlarmFrame(F("C1:"), UPS1_ALARM_INPUT_PIN);
    sendAlarmFrame(F("C2:"), UPS2_ALARM_INPUT_PIN);
  }

  if (temperatureSensor.ready()) {
    Serial.print(F("T:"));

    if (temperatureSensor.readTemp()) {
      Serial.println(temperatureToTenths(temperatureSensor.getTempRaw()));
    } else {
      Serial.println(TEMPERATURE_ERROR_VALUE);
    }

    temperatureSensor.requestTemp();
  }
}
