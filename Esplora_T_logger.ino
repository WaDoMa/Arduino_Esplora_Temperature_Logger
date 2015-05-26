/*
 Arduino Esplora TFT Temperature Display and Logger

 Reads the temperature of the Esplora on-board thermal sensor and displays it on an attached
 LCD screen and stores it onto a SD.card. Update roughly every second.

 This code is public domain.
 April 2013 by WaDoMan
https://github.com/WaDoMa
*/

// includes the necessary libraries
#include <Esplora.h>
#include <TFT.h>  // Arduino LCD library
#include <SPI.h>
#include <SD.h>  //Arduino SD-card library
#include <stdio.h>

#define SD_CS    8  // the Esplora pin connected to the chip select line for SD card

File handleFile;
char logfilename[]={"T_log.txt"};  // declaration and definition of log file name
int sd_ready;  // flag globally indicating, if SD card is available or not

void setup()  //initial, executed once sub named "setup" - automatically called by Arduino
{
  EsploraTFT.begin();   // initializes the display
  EsploraTFT.background(0, 0, 0);  // sets the background to black
  EsploraTFT.stroke(255, 255, 255);  // sets the text color white
  EsploraTFT.setTextSize(2);  // sets the text size for heading
  EsploraTFT.text("Temperature:", 0, 0);  // writes heading - remains static
  EsploraTFT.setTextSize(4);  // sets the text size for values
  EsploraTFT.text("\xF7\C", 110, 90);  // writes the unit - remains static; weird issue: the "°"-character has the code 0xF8 within the standard ASCII-table, but within the Esplora.TFT library, it is represented by 0xF7!
  
  analogReference(INTERNAL);  //Selects internal reference voltage (here: ATmega32u4 with 2.56 V) in order to get independent from inaccurate USB supply voltage. Refer also to http://www.atmel.com/images/doc7766.pdf , p.292.
  sd_ready=initSD(logfilename);  // prepares everything for writing to SD-card, if possible - the status of writability is stored to the "sd-ready" flag
 }

void loop()  //continiuously executed sub named "loop" - repeatedly called by Arduino
{
  char prectempPrintout[5];  // array to hold the precise temperature data string
  
  String precisetemperature = String(custom_readTemperature(DEGREES_C));  // reads the precise temperature [°C] and stores it in a String
  precisetemperature.toCharArray(prectempPrintout, 5);  // converts the string to a char array
  EsploraTFT.stroke(255, 255, 255);  // sets the text color to white
  EsploraTFT.text(prectempPrintout, 0, 90);   // prints the precise temperature two lines below the heading
  
  delay(1000);
  EsploraTFT.stroke(0, 0, 0);  // sets the text color to black
  EsploraTFT.text(prectempPrintout, 0, 90); 
  logValue(logfilename,prectempPrintout);  //appends char string holding the temperature to the logfile 

  if(Esplora.readButton(SWITCH_DOWN) == LOW)  //Checks, if Switch 1 is being pressed by human. (Turns low, if so.) 
  {
    Esplora.writeRGB(0,16,0);  //Signals data export via tty.
    file2tty(logfilename);  //Streams hole file to the virtual serial console for testing or easy data export to PC.
  }
  else
  {
    Esplora.writeRed(0);
  }

}

int logValue (char *filename,char *valueCharArray)
{
  if (sd_ready==1)  // write only, if SD card was initialy detected
  {
    handleFile = SD.open(filename, FILE_WRITE);
    if (handleFile)  // if the file could be opened, write to it:
    {
      char buf[100];
      buf[0]=0;
      Esplora.writeRGB(0, 0, 16); // found? yes - blue LED status
      sprintf(buf,"%s\n",valueCharArray);
      handleFile.print(buf);
      //handleFile.println(valueCharArray);  // write temperature    
      handleFile.close();  // close the file:
      Esplora.writeRGB(0, 0, 0); // reset LED in order to save energy
      return 1;  // return 1 for stating successful writing
    } 
    else
    {   
      Esplora.writeRGB(16, 0, 0); // opening error - red LED status
      Esplora.writeRGB(0, 0, 0); // reset LED in order to save energy
      return -1;  // return -1 for stating of no writing
    }
    return 0;  // return 0 for stating of no action because of not exisisting SD card
  }
}

float custom_readTemperature(const byte)  //Custom "Esplora.readTemperature()"-like function returning the result as a float value. Returns only °C! Made for higher precision, a more flexible application and a deeper understanding of the readout procedure  
{
  //Caution! For good precission the following measures need to be taken:
  // 1.) The reference voltage for the ADC must be excactly know and to be stable. (Rather onlike for the Esplora at stock configuration. Use "INTERNAL" setting instead!)
  // 2.) The actual offset and gain of the TMP36 needs to be determined by calibration measurements. Stock values are not only default values. Sensors are neither individually calibrated nor adjusted from factory.
  // Remark: The 10bit ADC in combination with the relatively low gain of the "TMP36" limits temperature resolution by quantisation effects. 
  const byte MUX_ADDR_PINS[] = { A0, A1, A2, A3 };  //Constants taken from "Esplora.cpp". Not defined globally, so it had to be taken from there.
  const byte MUX_COM_PIN = A4;   //Constant taken from "Esplora.cpp". Dito.
  
  const byte CH_TEMPERATURE = 6;    //CH_TEMPERATURE is already defined in "Esplora.h", but needed to by copied to the function as it is not globally accessible.
  byte channel = CH_TEMPERATURE;  //Loads constant into variable. Could also have been written directly into the "digitalWrite" commands in below, but was left for better comparability to orgininal code in "Esplore.cpp". 
  
  float temperature; 
  int ADC_range = 1024;  //Range of ADC-values being respresentable by the 10 bits of the ATMega-µC. The "unit" is [digits].
  float aref_voltage = 2.56;  //Reference voltage used by the ADC. The Unit is [V]. Here: 2.56 V by the INTERNAL voltage reference source (--> recommendation!). Caution - the default setting for "analogReference" is "DEFAULT" and means that the ADC reference voltage is provided by the USB host device and thus varying.
  float sensor_offset = 0.5;  //The offset of the temperature sensor "TMP36" being used on the Esplora board: 0.5 V (This means 0.5 V output at 0 °C.)
  float sensor_gain = (float)1/100;  //The gain of the temperature sensor "TMP36" being used on the Esplora board: 1 V per 100 °C (=1/100)
    
  digitalWrite(MUX_ADDR_PINS[0], (channel & 1) ? HIGH : LOW);  //Code from "Esplora.cpp" configuring the GPIO-Ports
  digitalWrite(MUX_ADDR_PINS[1], (channel & 2) ? HIGH : LOW);
  digitalWrite(MUX_ADDR_PINS[2], (channel & 4) ? HIGH : LOW);
  digitalWrite(MUX_ADDR_PINS[3], (channel & 8) ? HIGH : LOW);
  
  long ADC_readout = analogRead(MUX_COM_PIN);  //Acquires the current ADC readout. The "unit" is [digits]. 
  
  temperature = ((((float)ADC_readout*aref_voltage)/(float)ADC_range)-sensor_offset)/sensor_gain;  //This formula with correct units: T [°C] = (((ADC_readout [digits] * aref_voltage [V]) / ADC_range [digits]) - sensor_offset [V]) / sensor_gain [°C/V]
  return temperature;
}

int initSD(char *filename)
{
  if (!SD.begin(SD_CS))  // initialise SD card (check, if it is there and accessible)
  {
    Esplora.writeRGB(16, 0, 0); // no - red LED status
    return 0;  // sd card not available
  }
  else
  {
    if (SD.exists(filename))  // check, if old log file is there
    {
      SD.remove(filename);  // yes? delete the old log file
    }
    Esplora.writeRGB(0, 16, 0); // yes - green LED status
    return 1;  // sd card available
  }
}

void file2tty (char *filename)
{
  handleFile = SD.open(filename);  // re-open the file for reading:
  if (handleFile)   // if the file could be opened, read it
  {
    Serial.print("Content of ");   //Prints heading
    Serial.println(filename);
    while (handleFile.available())  // reads everything from the file until end of file (EOF)
    {
      Serial.write(handleFile.read());
    }
    handleFile.close();  // closes file
  } 
  else
  {
    Serial.println("error opening file");  // if the file couln not be opened, report an error
  }
}
