//ESP32 Dev Module


//#include <Arduino.h>
//#include <math.h>  // check!!!
//#include <spinlock.h>

#include "MAX11300registers.h"
#include <SPI.h>
SPIClass *SPIV;

//#include "MAX11300.h"
//https://github.com/logos-electromechanical/MAX11300Arduino/blob/master/src/MAX11300registers.h


#define NUM_CHARS_INPUT 32
#define DRIVER_NAME "PIXI click driver"
#define CONV_PIN 17
#define SELECT_PIN 5
#define MAX_12BIT_OUT 4095  //2^12-1
#define TEMP_LSB (0.125 / 16)

//Global variables ...

//const byte numChars = NUM_CHARS_INPUT;
char serialChars[NUM_CHARS_INPUT];  // an array to store the received data
char tempChars[NUM_CHARS_INPUT];    // temporary array for use when parsing
char commandFromSerial[NUM_CHARS_INPUT] = { 0 };
char message1FromPC[NUM_CHARS_INPUT] = { 0 };
int integerFromPC = 0;
int integer2FromPC = 0;

boolean newData = false;

void serialReadLine(char *serialChars);
void parseData();
void showParsedData();

unsigned long lastTmpReadTime_ms = 0;
int tmpReadPeriod_ms = 1000;  //read temperature every tmpReadPeriod_ms
unsigned long time_ms;

// long, int, float, char, word (A word can store an unsigned number of at least 16 bits (from 0 to 65535).)

const int led = 1;  // pin with a LED
/*
pinMode_t dac = pinMode_t(MAX_FUNCID_DAC);  //pin mode DAC
ADCref_t reference_internal = ADCref_t(ADCInternal);
MAX11300 MAX = MAX11300(&SPI, CONV_PIN, SELECT_PIN);
*/

void setup() {
  // put your setup code here, to run once:
  //pinMode (LED_BUILTIN, OUTPUT);  // setting the valtage  // initialize digital pin LED_BULETIN as an output
  Serial.begin(115200, SERIAL_8N1);  // start serial communication
  // Serial.begin vs arduinoSerial.begin ??
  pinMode(SELECT_PIN, OUTPUT);

  SPIV = new SPIClass(VSPI);
  //SPIV->begin();
  SPIV->begin(SCK, MISO, MOSI, SS);  //Start SPI communication
  /*
  Serial.print("SCK ");
  Serial.println(SCK);
  Serial.print("MISO ");
  Serial.println(MISO);
  Serial.print("MOSI ");
  Serial.println(MOSI);
  Serial.print("SS ");
  Serial.println(SS);
  */

  uint16_t id = read2ByteSPI(0);  // get device ID
  Serial.print("Chip id:");
  Serial.println(id);

  setDACreferenceInternal();
  setDACpins();
  enableINTTEMP();

  //digitalWrite(SELECT_PIN, LOW);
  //SPIV->beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));  //MSBFIRST
  //digitalWrite(SELECT_PIN, HIGH);

  /*
  //bool MAX11300::setPinMode(uint8_t pin, pinMode_t mode)
  for (int i = 0; i < 20; i++) {
    MAX.setPinMode(i, dac);
    //bool MAX11300::setPinADCref (uint8_t pin, ADCref_t reference)
    MAX.setPinADCref (i, reference_internal);
  }
*/
  delay(100);  // wait for ESP32 to be ready
  Serial.println(DRIVER_NAME);
}

void loop() {

  if (Serial.available()) {
    serialReadLine(serialChars);
    //Serial.println("Dobio podatak");
    Serial.println(serialChars);
    //newData = true;
    strcpy(tempChars, serialChars);
    parseData();
  }

  uint16_t data = 0;

  time_ms = millis();
  if ((unsigned long)(time_ms - lastTmpReadTime_ms) >= tmpReadPeriod_ms) {
    lastTmpReadTime_ms = time_ms;

    float temp = readTemperature_C(MAX_TMPINTDAT);  //read temperature
    Serial.print("temp: ");
    Serial.print(temp);
    Serial.println(" C");
    /*
    data = read2ByteSPI(MAX_DEVCTL); //read Device control register
    Serial.print("DEVCTL: ");
    Serial.println(data);
    printBin(data,16);
    Serial.write('\n');
    //Serial.print("device id: ");
    //Serial.println(0x0424);
    */
  }
}

uint16_t read2ByteSPI(uint16_t address) {

  uint16_t value = 0;
  SPIV->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS, LOW);
  //Serial.println("SPIV");
  SPIV->transfer((address << 1) + 1);  //read
  value = (uint16_t)SPIV->transfer(0) << 8;
  value |= (uint16_t)SPIV->transfer(0);

  digitalWrite(SS, HIGH);
  SPIV->endTransaction();
  return value;
}

void write2ByteSPI(uint16_t address, uint16_t value) {
  SPIV->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS, LOW);
  //Serial.println("SPIV");
  SPIV->transfer((address << 1) + 0);  //write
  SPIV->transfer(highByte(value));
  SPIV->transfer(lowByte(value));

  digitalWrite(SS, HIGH);
  SPIV->endTransaction();
}

// read input from GUI
void serialReadLine(char *serialChars) {
  static byte n = 0;
  char rc;
  if (Serial.available() > 0)
    while ((rc = Serial.read()) != '\n') {
      *(serialChars + n) = rc;
      n++;
      if (n >= NUM_CHARS_INPUT) {
        n = NUM_CHARS_INPUT - 1;
      }
    }                         //end while
  *(serialChars + n) = '\0';  // terminate the string
  n = 0;
}

// split the data
void parseData() {  // split the data into its parts

  char *strtokIndx;  // this is used by strtok() as an index

  strtokIndx = strtok(tempChars, " ");    // get the first part - the string
  strcpy(commandFromSerial, strtokIndx);  // copy it to commandFromSerial

  if (strcmp(commandFromSerial, "?") == 0 || strcmp(commandFromSerial, "help") == 0) {
    //print help
    Serial.println(DRIVER_NAME);
    Serial.println("I accept commands:");
    Serial.println("\t ? or help  - to get this output");
    Serial.println("\t set <int:ch number> <int:desired output 0-2^12>");
    Serial.println("\t readaddress <hex/int:register address 0-0x73>");
    Serial.println("\t temperature?");
    Serial.println("\t whois? - return name of the driver");

    return;
  }

  if (strcmp(commandFromSerial, "set") == 0) {
    //set ch output
    Serial.print("Got: set ");
    if ((strtokIndx = strtok(NULL, " ")) != NULL)  //get ch number
    {

      uint8_t pin = atoi(strtokIndx);  // convert this part to an integer
      Serial.print(pin);
      Serial.print(" ");
      if ((strtokIndx = strtok(NULL, " ")) != NULL)  //get number
      {
        uint16_t value = atoi(strtokIndx);
        Serial.println(value);
        //void setCHoutput(uint16_t pin, uint16_t value)
        setCHoutput(pin, value);
      }
    }
    Serial.println();  //clear a line
  }                    //end if set
  if (strcmp(commandFromSerial, "whois?") == 0) {
    //print DRIVER_NAME
    Serial.println(DRIVER_NAME);
    return;
  }  //end if whois?

  if (strcmp(commandFromSerial, "readaddress") == 0) {  //read register value and print

    if ((strtokIndx = strtok(NULL, " ")) != NULL)  //get ch number
    {
      uint16_t address = (uint16_t)strtoul(strtokIndx, NULL, 0);  //atoi(strtokIndx);
      uint16_t value = read2ByteSPI(address);
      Serial.print("Register adr: 0x");
      Serial.print(address, HEX);
      Serial.print(" -> ");
      printBin(value, 16);
      Serial.print(" -> 0x");
      Serial.print(value, HEX);
      Serial.print(" -> ");
      Serial.print(value);
      Serial.println();
    }
  }

  if (strcmp(commandFromSerial, "temperature?") == 0) {
    //uint16_t temp_b = read2ByteSPI(MAX_TMPINTDAT);
    float temp = readTemperature_C(MAX_TMPINTDAT);  //((int16_t)(temp_b << 4) * TEMP_LSB);
    Serial.print("Internal temperature: ");
    Serial.print(temp);
    Serial.println(" C");
  }

  //command not recognised
  Serial.print(commandFromSerial);
  Serial.println(" not recognised.");
}

float readTemperature_C(uint16_t address) {
  return ((int16_t)(read2ByteSPI(address) << 4) * TEMP_LSB);  //MAX_TMPINTDAT
}

void setCHoutput(uint16_t pin, uint16_t value) {
  //DAC adress 0x60 - 0x73
  uint16_t address = 0x60 + pin;
  write2ByteSPI(address, value);
}

void setDACpins() {
  //address 0x20-0x33 range of 20 addresses
  //0101: Mode 5 - Analog output for DAC (Figure 6)
  //010 -5 to +5 - Range
  // 000 - ADC only # of samples
  // 00000 - ASSOCIATED PORT
  uint16_t data = 0x5200;  // B01010 010 000 00000;
  for (uint16_t i = 0; i < 20; i++) {
    write2ByteSPI(0x20 + i, data);
  }
}

void setDACreferenceInternal() {

  uint16_t data = read2ByteSPI(MAX_DEVCTL);
  //data is current state of the register

  data |= B1000000;                 //set bit 6 to 1
  write2ByteSPI(MAX_DEVCTL, data);  //write to config register
}

void enableINTTEMP() {
  uint16_t data = read2ByteSPI(MAX_DEVCTL);
  //data is current state of the register

  data |= 0x0100;                   //set bit 8 to 1, 8->internal temperature, 9->external 1, 10->external 2
  write2ByteSPI(MAX_DEVCTL, data);  //write to config register
}

void printBin(uint16_t aByte, int digits) {
  for (int8_t aBit = digits - 1; aBit >= 0; aBit--)
    Serial.write(bitRead(aByte, aBit) ? '1' : '0');
}