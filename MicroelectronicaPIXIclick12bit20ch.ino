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

// long, int, float, char, word (A word can store an unsigned number of at least 16 bits (from 0 to 65535).)

const int led = 1;  // pin with a LED
/*
pinMode_t dac = pinMode_t(MAX_FUNCID_DAC);  //pin mode DAC
ADCref_t reference_internal = ADCref_t(ADCInternal);
MAX11300 MAX = MAX11300(&SPI, CONV_PIN, SELECT_PIN);
*/
byte spi[3];

void setup() {
  // put your setup code here, to run once:
  //pinMode (LED_BUILTIN, OUTPUT);  // setting the valtage  // initialize digital pin LED_BULETIN as an output
  Serial.begin(115200);  // start serial communication
  // Serial.begin vs arduinoSerial.begin ??
  pinMode(SELECT_PIN, OUTPUT);

  SPIV = new SPIClass(VSPI);
  //SPIV->begin();
  SPIV->begin(SCK, MISO, MOSI, SS);  //Start SPI communication
  Serial.print("SCK ");
  Serial.println(SCK);
  Serial.print("MISO ");
  Serial.println(MISO);
  Serial.print("MOSI ");
  Serial.println(MOSI);
  Serial.print("SS ");
  Serial.println(SS);

  uint16_t id = read2byteSPI(0);
  Serial.print("Chip id:");
  Serial.println(id);

  setDACreferenceInternal();
  setDACpins();
  for (int pin = 0; pin < 19; pin++) {
    setCHoutput(pin, 2048);
  }

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
    Serial.println("Dobio podatak");
    Serial.println(serialChars);
    //newData = true;
    strcpy(tempChars, serialChars);
    parseData();
  }

  spi[0] = 0x08;  //B00110000;  //Write to and Update (Power Up)
  spi[1] = 0;     // highByte(out);
  spi[2] = 0;     //lowByte(out);
  uint16_t data = 0;
  //digitalWrite(SELECT_PIN, HIGH);

  /*
   data= read2byteSPI(MAX_TMPINTDAT);//read temperature
  

  Serial.print("temp: ");
  Serial.println(data);
  //Serial.print("device id: ");
  //Serial.println(0x0424);
  delay(500);
  */
}

uint16_t read2byteSPI(uint16_t address) {

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
    Serial.println("\tset <int:ch number> <int:desired output 0-2^12>");
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
  }

  if (strcmp(commandFromSerial, "temperature?") == 0) {
    uint16_t temp_b = read2byteSPI(MAX_TMPINTDAT);
    int16_t temp = ((int16_t)(temp_b << 4) * TEMP_LSB);
    Serial.print("Internal temperature: ");
    Serial.print(temp);
    Serial.println(" C");
  }
}

void setCHoutput(uint16_t pin, uint16_t value) {
  //DAC adress 0x60 - 0x73
  SPIV->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS, LOW);
  //Serial.println("SPIV");
  SPIV->transfer(((0x60 + pin) << 1) + 0);  //write

  SPIV->transfer(highByte(value));
  SPIV->transfer(lowByte(value));

  digitalWrite(SS, HIGH);
  SPIV->endTransaction();
}

void setDACpins() {
  //address 0x20-0x33 range of 20 addresses
  //0101: Mode 5 - Analog output for DAC (Figure 6)
  //010 -5 to +5 - Range
  // 000 - ADC only # of samples
  // 00000 - ASSOCIATED PORT
  uint16_t data = 0x5200;  // B01010 010 000 00000;
  for (uint16_t i = 0; i < 20; i++) {
    SPIV->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(SS, LOW);
    //Serial.println("SPIV");
    SPIV->transfer(((0x20 + i) << 1) + 0);  //write

    SPIV->transfer(highByte(data));
    SPIV->transfer(lowByte(data));

    digitalWrite(SS, HIGH);
    SPIV->endTransaction();
  }
}

void setDACreferenceInternal() {
  uint16_t data = 0;
  SPIV->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS, LOW);
  //Serial.println("SPIV");
  SPIV->transfer((0x10 << 1) + 1);  //read
  data |= (uint16_t)SPIV->transfer(0) << 8;
  data |= (uint16_t)SPIV->transfer(0);

  digitalWrite(SS, HIGH);
  SPIV->endTransaction();
  //data is current state of the register

  data |= B1000000;  //set bit 6 to 1

  SPIV->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS, LOW);
  //Serial.println("SPIV");
  SPIV->transfer((0x10 << 1) + 0);  //write

  SPIV->transfer(highByte(data));
  SPIV->transfer(lowByte(data));

  digitalWrite(SS, HIGH);
  SPIV->endTransaction();
}

// monitor_speed = // Serial monitor

/*
char Set Chanel Num;                     // an array to store the received data
char set_ch_num;                       // temporary array for use when parsing

void loop() {
  if (Serial.available()) {
    String received = Serial.readStringUntil('')
  }
}

void loop() {
  if (espSerial.available()) {
    String received = espSerial.readStringUntil('');
    received.trim(); // remove any trailing whitespace
    Serial.println("Received from Arduino: " + );

  }
}

*/