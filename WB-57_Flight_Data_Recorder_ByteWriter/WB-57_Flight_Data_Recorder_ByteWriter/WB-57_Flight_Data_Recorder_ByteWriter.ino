#include <Adafruit_LIS3DH.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
//#include <SD.h>
#include <SdFat.h>
#include <Adafruit_MAX31865.h>

//Declarations for imported hardware firmware classes

//Weight on Wheels Sensor
#define WEIGHT_ON_WHEELS 13 //pin for the Weight on Wheels

//Temp Sensor
#define MAX31865_CS 6//7 // CS pin
#define RREF 430.0 // The value of the Rref resistor. Use 430.0!
Adafruit_MAX31865 max = Adafruit_MAX31865(MAX31865_CS); // Use hardware SPI, just pass in the CS pin

//Pressure Sensor
#define BMP_CS 5//6
Adafruit_BMP280 bme(BMP_CS); // hardware SPI

// Accelerometer
#define LIS3DH_CS 7
Adafruit_LIS3DH lis = Adafruit_LIS3DH(LIS3DH_CS);

//SD Reader
#define SD_CS SDCARD_SS_PIN

//Internal Heater
#define HEATER_PIN 4 //pin for the PWM output for the heater
char dutyCycle;

//Fail Light LOW = all good, HIGH = fail
#define FAIL_LIGHT 12

//Software Instance variables
bool isRecording; //whether or not the recorder is running
int totalEntries = 0; //total number of recorded entries
unsigned long lastTime; //the time since the last record call
unsigned long lastTimeHeat; // the time since the last heat cycle
double lastTempDeviance; // temp error from last cycle to be used to determine slope of error.
double lastPWM; // last cycle's PWM output before making it an integer
String dataString = ""; // holds the String of one line to be written to the CSV file
double internalTemp; //internal temperature of the box
double lastIntTemp = 0; // last integer temperature
double staticTempCounter = 0; // temperature counter, increments if there is a sensor fails
double extTemp = 0;
SdFat SD;
File sensorData; //pointer to the file on the SD
byte buf[512]; // A buffer to be written to the SD
int index_buf = 0; // Last written data point
byte dataFile[6];
bool finished = false;


//Constants
const int POLLING_RATE = (int)((1/(double)400)*1000); //the polling rate, expressed in miliseconds (400Hz = 1/400ms)
const int HEAT_CYCLE = 15000; //sets a heat cycle time of 15 sec (in millisec)
const int DESIRED_TEMP = 15; //desired temperature of the box, what the pwm of the heater works toward, in celsius
const String CSV_FILENAME = "DATA.CSV"; //the file name of the CSV file on the SD
const double P_GAIN = 1; // proportinal gain for the heater control
const double D_GAIN = -0.4; //derivative gain for the heater control
const double TOUCH_TEMP_LIMIT = 50; // Touch temp limit on box
const double EXT_TEMP_LIMIT = 45; // Touch temp limit for box using ext temp probe


//class Defintions
/**
 * DataBlock class to represent a collection of all current readings from the sensors
 */

class DataBlock
{
  public:
    double extTemp;
    double pressure;
    double intTemp;
    double accelX;
    double accelY;
    double accelZ;
    byte dataFile[5];
    DataBlock();
};

/*
struct DataBlock
{
    double extTemp;
    double pressure;
    double intTemp;
    double accelX;
    double accelY;
    double accelZ;
    //byte data[5];
    //DataBlock();
};
*/

//constructor for the DataBlock object
DataBlock::DataBlock(void)
{
  extTemp = 0.0;
  pressure = 0.0;
  intTemp = 0.0;
  accelX = 0.0;
  accelY = 0.0;
  accelZ = 0.0;
};

DataBlock data;


void setup()
{
  initialization();
}

void initialization()
{
  //Initialize the serial bus, needed to write back to console
  Serial.begin(12000000);
  //SPI.begin();
  //SPI.beginTransaction(SPISettings(120000000, MSBFIRST, SPI_MODE0));
  while (!Serial);     // pause until serial console opens
  Serial.println("Initializing software...");
  //pinMode(12, OUTPUT);
  //digitalWrite(12, LOW);

  isRecording = true;
  totalEntries = 0;
  lastTime = millis();
  internalTemp = 0;

  initSensors();
  initCSV();
  initHeater();
  Serial.println("Software Initialized. Beginning recording...");
}

void record()
{
   DataBlock currentReadings = pollSensors();
   
   //pollSensors();
   writeToCSV(currentReadings);
   //printToConsole(currentReadings);
   internalTemp = data.intTemp;
   //Serial.println(totalEntries);
   totalEntries++;
}

void checkForWeightOnWheels() // pin 13 - when there is weight on wheels = 1, lift-off = 0
{
// Serial.print("Entered checkForWeightOnWheels function ");
 // if (totalEntries < 100) //TODO: fill in once we know how to get the plane's wheels up signal
  if (!digitalRead(WEIGHT_ON_WHEELS))
  {
    isRecording = true;
  }
  else
  {
    //isRecording = false;
  }
}

DataBlock pollSensors()
{
  DataBlock currDataBlock;
  //data->extTemp = readExtTemp();
  double currExtTemp = readExtTemp();  
  currDataBlock.extTemp = currExtTemp;
  //data->pressure = readPressure();
  double currPres = readPressure();
  currDataBlock.pressure = currPres;

  //data->intTemp = readIntTemp();
  double currIntTemp = readIntTemp();
  currDataBlock.intTemp = currIntTemp;
  failTempSensor(data);

  //data->currAccelX = readAccelX();
  double currAccelX = readAccelX();
  currDataBlock.accelX = currAccelX;

  //data->accelY = readAccelY();
  double currAccelY = readAccelY();
  currDataBlock.accelY = currAccelY;

  //data->accelZ = readAccelZ();
  double currAccelZ = readAccelZ();
  currDataBlock.accelZ = currAccelZ;

  return currDataBlock;
}

double readExtTemp()
{
  double currExtTemp = max.temperature(100, RREF);
  return currExtTemp;
}

double readIntTemp()
{
  double currIntTemp = bme.readTemperature();
  return currIntTemp;
}

double readPressure()
{
  double currPres = bme.readPressure();
  return currPres;
}

double readAccelX()
{
  sensors_event_t event;
  lis.getEvent(&event);
  double currAccelX = event.acceleration.x;
  return currAccelX;
}

double readAccelY()
{
  sensors_event_t event;
  lis.getEvent(&event);
  double currAccelY = event.acceleration.y;
  return currAccelY;
}

double readAccelZ()
{
  sensors_event_t event;
  lis.getEvent(&event);
  double currAccelZ = event.acceleration.z;
  return currAccelZ;
}

void printToConsole(DataBlock dataToWrite)
{
  //Print Temperature data
  Serial.print("External Temp (°C): ");
  Serial.print(dataToWrite.extTemp);
  Serial.print(" ");

  Serial.print("Pressure (Pa): ");
  Serial.print(dataToWrite.pressure);
  Serial.print(" ");

  Serial.print("Internal Temp (°C): ");
  Serial.print(dataToWrite.intTemp);
  Serial.print(" ");

  Serial.print("Acceleration X: ");
  Serial.print(dataToWrite.accelX);
  Serial.print(" ");

  Serial.print("Acceleration Y: ");
  Serial.print(dataToWrite.accelY);
  Serial.print(" ");

  Serial.print("Acceleration Z: ");
  Serial.print(dataToWrite.accelZ);
  Serial.print(" ");

  Serial.println();
}

void writeToCSV(DataBlock dataToWrite)
{
  // build the data string *Not Needed*
  dataString = (String)dataToWrite.extTemp + "," + (String)dataToWrite.pressure + "," + (String)dataToWrite.accelX + "," + (String)dataToWrite.accelY + "," + (String)dataToWrite.accelZ + "," + (String)millis();
  /*
   * Write data in bytes to the file. Theoretically this should optimize performance
   * and require fewer processor operations.
   * 
   * It would be better if the sensor readings were not passed to this function as a
   * DataBlock object. Using a struct would reduce our overhead. 
   * 
   * Furthermore, if we used pointers with the struct instead of creating a new DataBlock object 
   * every time to pass to this function we would save valuable time. 
  */

  
  //Ideally this would appear in a struct
  //Cast the sensor values as bytes so that they write to the SD faster
  /*
  dataFile[0] = (byte)dataToWrite.extTemp;
  Serial.println((String)dataToWrite.extTemp + " Data File 1: " + dataFile[0]);
  dataFile[1] = (byte)dataToWrite.pressure;
  Serial.println((String)dataToWrite.pressure + "Data File 2: " + dataFile[1]);
  dataFile[2] = (byte)dataToWrite.accelX;
  dataFile[3] = (byte)dataToWrite.accelY;
  dataFile[4] = (byte)dataToWrite.accelZ;
  
  //Add the data to buf until it is full then write
  
  index_buf++;  
  for(int i = 0; index_buf < 512 && i < 5; i++){
    buf[index_buf] = dataFile[i];
  }
  */
    
    /*
     * Write buf to the file once it is full
     * NOTE: this buffer size may be so large that it takes to long to
     * write it at one time. Try 128, 32, etc to see if smaller is better
    */
    /*
    if(index_buf >= 511){sensorData.write(buf, 512); index_buf = 0;}  
    */
    
  
  /*
   *Alternative method that does not require translation from bytes to csv.
   * When sensorData is called the data is not written immediately written to the file. 
   * 
   * sensorData.flush() is called once every 50 data entries to limit the number of times  
   * flush is called. By decreasing the frequency of the flushes we increase the speed of 
   * recording.
   */
   
    sensorData.println(dataString);
    /*  
    if(totalEntries % 200 == 0){
      //Serial.println("wrote data");
      sensorData.flush();
    }
    */
   sensorData.flush();
  
  //Below just slows the code down... don't know what it was for anyway.
  /*
  String tempString = "";
  tempString = totalEntries;
  Serial.print("Row ");
  Serial.print(tempString);
  Serial.println(" - Datablock written to CSV");
  */
}

void initCSV()
{
  DataBlock data;
  
  //Serial.print("Initializing SD card...");
  pinMode(SD_CS, OUTPUT);

  // see if the card is present and can be initialized: We need to try to initlialize again if not successful
  if (!SD.begin(SD_CS))
  {
    Serial.println("\nCard failed, or not present");
    while (!SD.begin(SD_CS))
    {
      pinMode(SD_CS, OUTPUT);
    }
  }

  Serial.println("Done.");

  //Serial.print("Initializing CSV...");
  
  sensorData = SD.open(CSV_FILENAME, O_CREAT | O_WRITE);
  //Serial.println("FILE OPEN, DO NOT REMOVE CARD");
  if (sensorData)
  {
    sensorData.println("External Temperature (C), Barometric Pressure (Pa), Acceleration X, Acceleration Y, Acceleration Z");
    sensorData.flush();
  }
  else
  {
    Serial.println("\nError writing to file!");
  }
  //Serial.println("Done.");

  //Serial.println("Card initialized...");
}

void initSensors()
{
  Serial.print("Initializing Sensors...");

  //initialize the sensors on the serial bus

  //Temp Sensor
  if (!max.begin(MAX31865_3WIRE)) // set to 2WIRE or 4WIRE as necessary
  {
    Serial.println("\n could not start temperature sensor");
  }

  //Pressure and int temp sensor
  if (!bme.begin())
  {
    Serial.println("\nCould not find a valid BMP280 sensor, check wiring!");
    //while (1);
  }

  //Accelerometer
  if (! lis.begin(0x18)) // change this to 0x19 for alternative i2c address
  {
    Serial.println("\nCouldn't start accelerometer");
  }
  lis.setRange(LIS3DH_RANGE_4_G);   // 2, 4, 8 or 16 G!

  Serial.println("Done.");
}

void initHeater()
{
  Serial.print("Initializing heater...");
  dutyCycle = 0;
  analogWrite(HEATER_PIN, dutyCycle);
  Serial.println("Done.");
}

void failTempSensor(DataBlock dataToCheck)

{
  if (dataToCheck.intTemp == lastIntTemp)
  {
      staticTempCounter++;
  }
  else
  {
      staticTempCounter = 0;
      lastIntTemp = dataToCheck.intTemp;
}

}
/**
 * manages the PWM of the heater pad. It works as follows:
 * -look at the current internal temperature of the box
 * -subtract the current temperature from the desired temperature (constant), this yields the deviance from desired, which will be a positive value if too cold, a negative value if too hot
 * -convert the deviance in steps ranging from 1 to 5 away from the desired temp (i.e. 1 unit (degree) too cold, 3 units too hot, 5 units too cold max, 5 units too hot max)
 * -scale the units to a percentage scale for the duty cycle (i.e. 1 unit too cold = 60% duty cycle, 3 units too hot = 20% duty cycle, 5 too cold = 100% DC, 5 too hot = 0% DC)
 * -scale the duty cycle from 0 to 255 based on the percentage value (i.e. 0% = 0, 100% = 255)
 */
void updateHeater(double intTemp) // Changes made to implement a PD controller  **********************************************************************
{
  double desired = DESIRED_TEMP;
  double actual = intTemp;
  double tempDeviance = desired - actual; //difference between actual and desired temps, positive if temp needs to go up
  double tempDerivative = lastTempDeviance - tempDeviance; // calculates the slope or derivative of the error
  double deltaPWM = tempDeviance * P_GAIN + tempDerivative * D_GAIN; //calculates what change needs to be made to the PWM output
  double PWM = lastPWM + deltaPWM;

  lastPWM = PWM; // saving this PWM value for next cycle
  lastTempDeviance = tempDeviance; // saving this temp error for next cycle

  int intPWM = (int)(PWM + 0.5); // converting PWM to an integer and adding 0.5 so it will round the value and not truncate

  if (intPWM > 255)
  {
    intPWM = 255;
  }
  else if (intPWM < 0)
  {
    intPWM = 0;
  }



  // * to make sure the box does not go too hot, we will impelment a back up temperature check. The logic should check the tempa and also make sure it isn't jsut bad data
  // *  from the backup sensor by using the external sensor also

     if ((staticTempCounter >= 10000) and (extTemp > EXT_TEMP_LIMIT))
     {
       intPWM = 0;
     }
//  Serial.print("intPWM is ");
//  Serial.println(intPWM);

  analogWrite(HEATER_PIN, intPWM);
}


void finish()
{
  Serial.print("Finished.");
  sensorData.close();
  finished = true;

  //TODO: sever connections to sensors nicely if needed
  //TODO: power down, or wait for signal to start recording again (don't know what wer are supposed to do here)
}


void loop()
{
   unsigned long currTime = millis();
   if (currTime >= lastTime + POLLING_RATE) // if possible we would like to loop at 400Hz, or every 2.5msec
    {
   //  while( !isRecording){
   //   checkForWeightOnWheels();
   //  }

      checkForWeightOnWheels(); //looking for the weight on wheels switch

       if (/*isRecording && */!finished) // if weight on wheel is switch is false, then isRecording will be true and we want to record
       {
           //Serial.println("recording");
           record();
       }
       /*else*/ 
       if (/*!isRecording &&*/ totalEntries > 100) //this stops the recording only if the plane has been in the air (gathered data)
                                                    //and no has weight on wheels
       {
           Serial.println("Recording Stopped ...");
           finish();
       }

       if (currTime >= lastTimeHeat + HEAT_CYCLE); // calling the heater control loop for the time set in HEAT_CYCLE (15 sec)
       {
          updateHeater(internalTemp);
          lastTimeHeat = currTime;
       }
       lastTime = currTime;
    }    
}
