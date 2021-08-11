/*
  Using the SparkFun smôl ARGOS ARTIC R2 Board
  By: Paul Clark
  SparkFun Electronics
  Date: July 30th 2021

  This example shows how to use the smôl ARTIC R2 for ARGOS satellite communication.
  The smôl ZOE-M8Q provides GNSS time and position.
  The smôl ESP32 is the board which runs this example.
  The smôl Power Board AAA or smôl Power Board LiPo provides the power.
  The system is placed into power-down between transmits.

  Feel like supporting our work? Buy a board from SparkFun!

  The smôl stack-up for this example is:
  smôl ZOE-M8Q:          https://www.sparkfun.com/products/18358
  smôl ARTIC R2:         https://www.sparkfun.com/products/18363
  smôl ESP32:            https://www.sparkfun.com/products/18362
  smôl Power Board LiPo: https://www.sparkfun.com/products/18359
  - or -
  smôl Power Board AAA:  https://www.sparkfun.com/products/18360

  The way the boards are stacked is important:

  OUT --- smôl ZOE-M8Q--- IN
                           |
   ________________________/
  /
  |
  OUT ---smôl ARTIC R2--- IN
                           |
   ________________________/
  /
  |
  OUT ---  smôl ESP32 --- IN
                           |
   ________________________/
  /
  |
  OUT ---  smôl Power --- IN

  Arranged like this:
  The ESP32 GPIO0 (Digital Pin 27) controls the power for the ARTIC R2
  The ESP32 GPIO1 (Digital Pin 26) controls the power for the ZOE-M8Q
  ARTIC R2 uses SPI Chip Select 0 (ESP32 Digital Pin 5)

  Make sure that you select the correct power board in the code below.
  Select:
    smolPowerLiPo myPowerBoard;
  or:
    smolPowerAAA myPowerBoard;
  below.

  Set the Board to: SparkFun ESP32 Arduino \ SparkFun ESP32 Thing
  
  This example:
    enables power for and begins the ZOE-M8Q;
    reads the ZOE-M8Q GNSS time, latitude and longitude;
    calculates the next satellite pass;
    goes into power-down until the next satellite pass;
    enables power for and begins (initializes) the ARTIC R2;
    reads and prints the ARTIC R2 TX and RX configuration;
    reads and prints the firmware status;
    sets the TCXO voltage;
    sets the TCXO warmup time;
    sets the satellite detection timeout to 60 seconds;
    sets the TX mode to ARGOS PTT-A2;
    sets the TX frequency;
    instructs the ARTIC R2 to Transmit One Package And Go Idle;
    keeps checking the MCU status until transmit is complete;
    repeats the message transmit numberTransmits times, repetitionPeriod seconds apart;
    repeats for the next satellite pass.

  The message contains the GNSS latitude and longitude in a compact form which ARGOS Web will understand.
  Please contact CLS / Woods Hole Group and ask them to apply the SPARKFUN_GPS template on ARGOS Web.
  The Latitude and Longitude will then be extracted, converted and displayed automatically when you view your data.
  The number of user bits is 56.
  Lat is encoded as 21 bits: the MSB is 0 for +ve latitude, 1 for -ve latitude (SOUTH); the unit is 0.0001 degrees. (Note: this is not two's complement!)
  Lon is encoded as 22 bits: the MSB is 0 for +ve longitude, 1 for -ve longitude (WEST); the unit is 0.0001 degrees. (Note: this is not two's complement!)

  Please log in to ARGOS Web https://argos-system.cls.fr/argos-cwi2/login.html
  and copy and paste the latest Satellite AOP (Adapted Orbit Parameters)
  into AOP below.

  From KINEIS-MU-2019-0094:
  Even though most of the satellites are maintained on their orbit thanks to maneuver capability (propulsion), they still drift with time
  because of the solar activity. The linear time margin parameter compensates for the drift by adding extra time to the computed satellite
  passes, allowing to use the same AOP data for up to 6 months, but the resulting passes have a much greater duration. Therefore, it is
  recommended to update the AOP at maximum every 2 or 3 months for the computation to be as accurate as possible and limit the time margin
  contribution in the satellite passes output calculations.

*/

// CLS will have provided you with a Platform ID for your ARGOS R2. Copy and paste it into PLATFORM_ID below.
// E.g.: if your Platform ID is 01:23:AB:CD then set PLATFORM_ID to 0x0123ABCD
const uint32_t PLATFORM_ID = 0x01234567; // Update this with your Platform ID

const uint32_t repetitionPeriod = 90; // The delay in seconds between transmits a.k.a. the repetition period (CLS will have told you what your repetition period should be)
const uint8_t numberTransmits = 5; // The number of transmit attempts for each pass (** Make sure this is >= 1 **)
const uint32_t tcxoWarmupTime = 10; // Start the transmit this many seconds early to compensate for the TCXO warmup time
const uint32_t articBootTimeMax = 10; // Define the maximum time the ARTIC should take to boot: 10 seconds for SPI, 3 seconds for Flash
const unsigned long maxGNSSfixTime = 60; // Define how long we should wait for a GNSS fix (seconds)
const float gnssFixDuty = 0.1; // When attempting a GNSS fix, define the on duty cycle

const uint8_t numARGOSsatellites = 8; // Change this if required to match the number of satellites in the AOP

// Copy and paste the latest AOP from ARGOS Web between the quotes and then carefully delete the line feeds
// Check the alignment afterwards - make sure that the satellite identifiers still line up correctly (or convertAOPtoParameters will go horribly wrong!)
// Check the alignment: " A1 6 0 0 1 2020 10 17 23 45 54  6891.715  97.4600   89.939  -23.755   95.0198  -2.04 MA A 5 3 0 2020 10 17 23 17 28  7195.659  98.5078  318.195  -25.342  101.3611   0.00 MB 9 3 0 0 2020 10 17 22 50 39  7195.586  98.7164  339.849  -25.339  101.3590   0.00 MC B 7 3 0 2020 10 17 22  3  0  7195.670  98.7232  352.079  -25.340  101.3608   0.00 15 5 0 0 0 2020 10 17 22 41 11  7180.481  98.7069  309.136  -25.259  101.0405  -0.11 18 8 0 0 0 2020 10 17 22  2 34  7226.005  99.0303  351.904  -25.498  102.0006  -0.80 19 C 6 0 0 2020 10 17 22 20 53  7226.397  99.1943  298.377  -25.499  102.0084  -0.51 SR D 4 3 0 2020 10 17 22 34 12  7160.232  98.5409  110.208  -25.154  100.6145  -0.12";
const char AOP[] =      " A1 6 0 0 1 2021  7 30 22 35  3  6890.719  97.4674  107.283  -23.750   94.9993  -3.10 MA A 5 3 0 2021  7 30 22 59 29  7195.508  98.4675  315.815  -25.341  101.3580   0.00 MB 9 3 0 0 2021  7 30 22 34  2  7195.620  98.7126  344.171  -25.340  101.3597   0.00 MC B 7 3 0 2021  7 30 23 27  9  7195.563  98.6827  331.038  -25.340  101.3586   0.00 15 5 0 0 0 2021  7 30 22 31  2  7180.277  98.6783  313.531  -25.259  101.0363  -1.45 18 8 0 0 0 2021  7 30 22 27  4  7225.691  98.9922  351.423  -25.497  101.9940  -0.92 19 C 6 0 0 2021  7 30 23 22  2  7226.181  99.1814  294.431  -25.498  102.0039  -0.84 SR D 4 3 0 2021  7 30 22  3 19  7160.122  98.5451  117.722  -25.153  100.6122  -0.23";

// Minimum satellite elevation (above the horizon):
//  Set this to 5 to 20 degrees if you have a clear view to the horizon.
//  45 degrees is really only suitable for urban environments and will severely limit the number of transmit windows...
float min_elevation = 15.0;

#include <sys/time.h> // Needed for ESP32 RTC

#include <SPI.h>

#include "SparkFun_ARGOS_ARTIC_R2_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_ARGOS_ARTIC_R2
ARTIC_R2 myARTIC;

#include <Wire.h> //Needed for I2C to ARTIC R2 GPIO and GNSS

#include "SparkFun_u-blox_GNSS_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_u-blox_GNSS
SFE_UBLOX_GNSS myGNSS;

#include "SparkFun_smol_Power_Board.h" // Click here to get the library: http://librarymanager/All#SparkFun_smol_Power_Board
//smolPowerAAA myPowerBoard; // Uncomment this line to use the smôl Power Board AAA
smolPowerLiPo myPowerBoard; // Uncomment this line to use the smôl Power Board LiPo

// Pin assignments for the smôl stack-up described above
int CS_Pin = 5;            // smôl CS0 = ESP32 Pin 5
int ARTIC_PWR_EN_Pin = 27; // smôl GPIO0 = ESP32 Pin 27
int GNSS_PWR_EN_Pin = 26;  // smôl GPIO1 = ESP32 Pin 26

// The ARTIC RESETB, INT1, BOOT and G8 signals are accessed through a PCA9536 I2C-GPIO chip on the smôl ARTIC R2

// Loop Steps - these are used by the switch/case in the main loop
// This structure makes it easy to jump between any of the steps
typedef enum {
  start_power_board,              // Start communication with the power board
  start_GNSS,                     // Power-on the GNSS
  wait_for_GNSS,                  // Wait for the GNSS time and position to be valid
  set_RTC,                        // Set the ESP32's RTC to GNSS time
  calculate_next_pass,            // Read the GNSS time, lat and lon. Calculate the next satellite pass
  wait_for_next_pass,             // Wait for the next satellite pass
  power_on_ARTIC,                 // Power-on the ARTIC, set the satellite detection timeout and TX mode
  wait_for_ARTIC_TX_to_start,     // Wait for the ARTIC transmit to start
  ARTIC_TX,                       // Start the ARTIC TX
  wait_for_ARTIC_TX_to_complete,  // Wait for the ARTIC to transmit
  power_down                      // Power-down until the next transmit window
} loop_steps;
loop_steps loop_step = start_power_board; // Initialize loop_step

// AS3-SP-516-2098-CNES specifies a ±10% 'jitter' on the repetition period to reduce the risk of transmission collisions
uint32_t nextTransmitTime; // Time of the next satellite transmission (before jitter is added)
uint32_t nextTransmitStart; // Time the next satellite transmission will start (including jitter and tcxoWarmupTime and articBootTime)
uint32_t nextTransmitPowerOn; // Time the ARTIC will be powered on for next satellite transmission will start (including jitter articBootTime)
uint8_t remainingTransmits; // Remaining number of satellite transmits
float lat_tx; // The latitude included in the transmitted message
float lon_tx; // The longitude included in the transmitted message
uint16_t powerDownDuration; // The power-down duration in seconds
bool articIsOn = false; // Flag to indicate if the ARTIC has been powered up
bool firstTransmit = true; // Flag to indicate if this is the first transmit
unsigned long startEvent; // Load this with millis() at the start of an event
float gnssLatitude; // Store the latitude (degrees)
float gnssLongitude; // Store the longitude (degrees)

void setup()
{
  delay(1000);
  
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("SparkFun smôl ARTIC R2 Example"));
  Serial.println();

  articIsOn = false; // Flag that the ARTIC is not yet powered-up
}

void loop()
{
  // loop is one large switch/case that controls the sequencing of the code
  switch (loop_step) {

    // ************************************************************************************************
    // Start communication with the power board
    case start_power_board:
    {
      Wire.begin();
    
      Serial.println(F("Starting communication with the power board..."));
      Serial.println();
    
      if (myPowerBoard.begin() == false) //Connect to the power board
      {
        Serial.println(F("smôl Power Board not detected at default I2C address. Please check the smôl stack-up and flexible circuits."));
        esp_sleep_enable_timer_wakeup(5000000); // Try again in five seconds
        esp_light_sleep_start();      
      }
      else
      {
        myPowerBoard.setWatchdogTimerPrescaler(SFE_SMOL_POWER_WDT_TIMEOUT_1s); // Set the WDT timeout (prescaler) to 1 second

        myPowerBoard.setADCVoltageReference(SFE_SMOL_POWER_USE_ADC_REF_VCC); // Select VCC as the voltage reference
        Serial.print(F("Battery voltage is "));
        Serial.println(myPowerBoard.getBatteryVoltage());
        loop_step = start_GNSS; // Move on
      }
    }
    break;
    
    // ************************************************************************************************
    // Wait for the GNSS time to be valid and for the position fix to be 3D
    case start_GNSS:
    {
      // Enable power for the ZOE-M8Q
      pinMode(GNSS_PWR_EN_Pin, OUTPUT);
      digitalWrite(GNSS_PWR_EN_Pin, LOW); // We need to pull the EN pin low to enable power for the GNSS
    
      // Give the ZOE time to start up
          esp_sleep_enable_timer_wakeup(1000000); //1 second
      esp_light_sleep_start();      

      Serial.println(F("Starting the u-blox GNSS module..."));
      Serial.println();
    
      //myGNSS.enableDebugging(); // Uncomment this line to see helpful debug messages on Serial
    
      if (myGNSS.begin() == false) //Connect to the ZOE-M8Q using Wire port
      {
        Serial.println(F("u-blox ZOE-M8Q GNSS not detected at default I2C address. Please check the smôl stack-up and flexible circuits."));
        powerDownDuration = 5; // Power-down for 5 seconds and try again (the power-down duration is a useful diagnostic)
        loop_step = power_down;
      }
      else
      {
        myGNSS.setI2COutput(COM_TYPE_UBX); //Set the I2C port to output UBX only (turn off NMEA noise)
        myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT); //Save (only) the current ioPortsettings to flash and BBR
        startEvent = millis(); // Record the time
        myGNSS.getTimeValid(); // Call getTimeValid twice to ensure we have fresh data
        loop_step = wait_for_GNSS;
      }
    }
    break;
    
    // ************************************************************************************************
    // Wait for the GNSS time to be valid and for the position fix to be 3D
    case wait_for_GNSS:
    {
      esp_sleep_enable_timer_wakeup(500000); //0.5 seconds
      esp_light_sleep_start();

      // Read the GNSS. Check that the time is valid.
      boolean timeValid = myGNSS.getTimeValid(); // Call getTimeValid twice to ensure we have fresh data
      Serial.print(F("GPS time is "));
      if (timeValid == false) Serial.print(F("not "));
      Serial.println(F("valid"));

      boolean dateValid = myGNSS.getDateValid();
      Serial.print(F("GPS date is "));
      if (dateValid == false) Serial.print(F("not "));
      Serial.println(F("valid"));

      boolean timeFullyResolved = myGNSS.getTimeFullyResolved();
      Serial.print(F("GPS time is "));
      if (timeFullyResolved == false) Serial.print(F("not "));
      Serial.println(F("fully resolved"));

      // Read the GNSS. Check that the position fix is 3D.
      uint8_t fixType = myGNSS.getFixType();
      Serial.print(F("GPS position fix type is "));
      Serial.println(fixType);
      Serial.println();

      if (timeValid && dateValid && timeFullyResolved && (fixType >= 3)) // Check if time, date and fix are valid
      {
        loop_step = set_RTC; // Move on
      }
      else if (millis() > (startEvent + (maxGNSSfixTime * 1000))) // Have we been powered up for maxGNSSfixTime seconds?
      {
        float offFraction = 1.0 - gnssFixDuty; // Calculate the powerDownDuration
        float period = maxGNSSfixTime / gnssFixDuty;
        powerDownDuration = (uint16_t)(period * offFraction);
        loop_step = power_down;        
      }
      else
      {
        Serial.println(F("Waiting for GPS time to be valid and the fix type to be 3D..."));
        Serial.println();
      }
    }
    break;

    // ************************************************************************************************
    // Read the time, latitude and longitude from GNSS
    // Set the ESP32's RTC
    case set_RTC:
    {
      // Read the GPS time, latitude and longitude. Convert to epoch.
      uint32_t epochNow = myARTIC.convertGPSTimeToEpoch(myGNSS.getYear(), myGNSS.getMonth(), myGNSS.getDay(), myGNSS.getHour(), myGNSS.getMinute(), myGNSS.getSecond()); // Convert GPS date & time to epoch
      Serial.print(F("GNSS time is: "));
      Serial.print(myARTIC.convertEpochToDateTime(epochNow));
      Serial.println(F(" UTC"));
      Serial.print(F("The number of seconds since the epoch is: "));
      Serial.println(epochNow);

      // Read the GPS lat and lon. Convert to float.
      gnssLatitude = ((float)myGNSS.getLatitude()) / 10000000; // Convert from degrees^-7
      gnssLongitude = ((float)myGNSS.getLongitude()) / 10000000; // Convert from degrees^-7

      // Print the lat and lon
      Serial.print(F("GPS Latitude is: "));
      Serial.println(gnssLatitude, 4);
      Serial.print(F("GPS Longitude is: "));
      Serial.println(gnssLongitude, 4);

      // Set the RTC
      struct timeval tv;
      tv.tv_sec = epochNow;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);

      // Read the RTC
      gettimeofday(&tv, NULL);
      Serial.print(F("RTC set to "));
      Serial.println(myARTIC.convertEpochToDateTime(tv.tv_sec));

      // Power down the GNSS now to save power - we'll use the RTC from now on
      digitalWrite(GNSS_PWR_EN_Pin, HIGH); // Disable power for the ZOE-M8Q

      loop_step = calculate_next_pass; // Move on
    }
    break;

    // ************************************************************************************************
    // Read the AOP
    // Read the time, latitude and longitude from GNSS
    // Calculate the time of the next satellite pass
    case calculate_next_pass:
    {
      // Read the AOP, convert into bulletin_data_t
      bulletin_data_t satelliteParameters[numARGOSsatellites]; // Create an array of bulletin_data_t to hold the parameters for all satellites
      if (myARTIC.convertAOPtoParameters(AOP, satelliteParameters, numARGOSsatellites) == false)
      {
        Serial.println("convertAOPtoParameters failed! Freezing...");
        while (1)
          ; // Do nothing more
      }

/*
      // Pretty-print the AOP for all satellites
      for (uint8_t i = 0; i < numARGOSsatellites; i++)
      {
        myARTIC.printAOPbulletin(satelliteParameters[i]);
        Serial.println();
      }
*/

      // Read the RTC
      struct timeval tv;
      gettimeofday(&tv, NULL);
      uint32_t epochNow = tv.tv_sec;

      Serial.print(F("RTC time is: "));
      Serial.print(myARTIC.convertEpochToDateTime(epochNow));
      Serial.println(F(" UTC"));
      Serial.print(F("The number of seconds since the epoch is: "));
      Serial.println(epochNow);

      // Predict the next satellite pass
      uint32_t nextSatellitePass = myARTIC.predictNextSatellitePass(satelliteParameters, min_elevation, numARGOSsatellites, gnssLongitude, gnssLatitude, epochNow);

      // Print the prediction
      Serial.print(F("The middle of the next satellite pass will be at: "));
      Serial.print(myARTIC.convertEpochToDateTime(nextSatellitePass));
      Serial.println(F(" UTC"));
      Serial.print(F("The number of seconds since the epoch will be: "));
      Serial.println(nextSatellitePass);

      if (numberTransmits >= 1)
      {
        nextTransmitTime = nextSatellitePass - (((numberTransmits - 1) / 2) * repetitionPeriod);
        nextTransmitStart = nextTransmitTime + random((-0.1 * repetitionPeriod), (0.1 * repetitionPeriod)); // Add the jitter
        nextTransmitStart -= tcxoWarmupTime; // Start the transmit early to compensate for the TCXO warmup time
        nextTransmitPowerOn = nextTransmitTime - articBootTimeMax - tcxoWarmupTime - (repetitionPeriod / 10); // Power-on early to compensate for the ARTIC boot time
        remainingTransmits = numberTransmits; // Remaining number of satellite transmits
        firstTransmit = true; // Flag that this is the first transmit
      }
      else
      {
        remainingTransmits = 0; // Remaining number of satellite transmits
      }

      // If transmits should have already started (i.e. nextTransmitPowerOn < epochNow)
      // then add repetitionPeriod to nextTransmitTime and decrement remainingTransmits
      // to avoid violating the repetitionPeriod on the next transmit
      while ((remainingTransmits > 0) && (nextTransmitPowerOn < epochNow))
      {
        nextTransmitTime += repetitionPeriod;
        nextTransmitStart += repetitionPeriod;
        nextTransmitPowerOn += repetitionPeriod;
        remainingTransmits--;
      }

      if (remainingTransmits >= 1)
      {
        Serial.print(F("Transmit attempt 1 of "));
        Serial.print(remainingTransmits);
        Serial.print(F(" will take place at: "));
        Serial.print(myARTIC.convertEpochToDateTime(nextTransmitTime));
        Serial.println(F(" UTC"));

        loop_step = wait_for_next_pass; // Move on
      }
      else
      {
        Serial.println(F("The transmission window was missed. Recalculating..."));
        Serial.println();
        // Leave loop_step unchanged so the next pass is recalculated
      }
    }
    break;

    // ************************************************************************************************
    // Wait until the next satellite pass
    case wait_for_next_pass:
    {
      // Go round this case twice per second
      esp_sleep_enable_timer_wakeup(500000); //0.5 seconds
      esp_light_sleep_start();
      
      // Read the RTC
      struct timeval tv;
      gettimeofday(&tv, NULL);
      uint32_t epochNow = tv.tv_sec;

      // Calculate how many seconds remain until we power-on for the next transmit
      int32_t secsRemaining = (int32_t)nextTransmitPowerOn - (int32_t)epochNow;

      // Count down in intervals of 100, then 10, then 1 second
      if (((secsRemaining >= 100) && (secsRemaining % 100 == 0)) ||
        ((secsRemaining < 100) && (secsRemaining % 10 == 0)) ||
        (secsRemaining < 10))
      {
        Serial.print(F("Power-on for next transmit will take place in "));
        Serial.print(secsRemaining);
        Serial.print(F(" second"));
        if (secsRemaining != 1) // Attention to detail is everything... :-)
          Serial.println(F("s"));
        else
          Serial.println();
      }

      // Power down completely if the time to the next transmit power-on
      // is more than maxGNSSfixTime and if this is the first transmit.
      if ((secsRemaining > maxGNSSfixTime) && firstTransmit)
      {
        if (secsRemaining <= (3 * 60 * 60)) // Is powerDownDuration <= 3 hours?
        {
          powerDownDuration = secsRemaining - maxGNSSfixTime; // Wake up before the next transmit
        }
        else
        {
          powerDownDuration = (3 * 60 * 60); // Limit powerDownDuration to 3 hours to try and preserve the GNSS ephemeris
        }
        loop_step = power_down;
      }
      // Check if we should power-down just the ARTIC between transmits
      // (If repetitionPeriod is short ( < (articBootTimeMax - tcxoWarmupTime) ) then nextTransmitPowerOn
      //  will be in the past and secsRemaining will be negative)
      else if ((secsRemaining > 0) && articIsOn)
      {
        digitalWrite(ARTIC_PWR_EN_Pin, LOW); // Disable power for the ARTIC
        articIsOn = false;
      }
      // Is it time to power-on the ARTIC?
      else if ((secsRemaining <= 0) && (articIsOn == false))
      {
        loop_step = power_on_ARTIC;
      }
      // Is it time to wait for TX to start?
      else if (secsRemaining <= 0)
      {
        loop_step = wait_for_ARTIC_TX_to_start;
      }
    }
    break;

    // ************************************************************************************************
    // Power-on the ARTIC
    case power_on_ARTIC:
    {
      SPI.begin();
    
      // Uncomment the next line to enable the helpful debug messages
      //myARTIC.enableDebugging(); // Enable debug messages to Serial
    
      Serial.println(F("Starting the ARTIC R2..."));
      Serial.println();

      bool success = true; // Flag if the ARTIC configuration is successful
    
      // Begin the ARTIC: enable power and upload firmware or boot from flash
      success &= myARTIC.beginSmol(CS_Pin, ARTIC_PWR_EN_Pin); // Default to using Wire to communicate with the PCA9536 I2C-GPIO chip on the smôl ARTIC R2
        
      success &= myARTIC.setTCXOControl(1.8, true); // Set the TCXO voltage to 1.8V and autoDisable to 1
      
      success &= myARTIC.setTCXOWarmupTime(tcxoWarmupTime); // Set the TCXO warm-up time
      
      success &= myARTIC.setSatelliteDetectionTimeout(60); // Set the satellite detection timeout to 60 seconds

      // Set the TX mode to ARGOS 3 PTT-A3
      ARTIC_R2_MCU_Command_Result result = myARTIC.sendConfigurationCommand(CONFIG_CMD_SET_PTT_A3_TX_MODE);
      myARTIC.printCommandResult(result); // Pretty-print the command result to Serial
      success &= (result == ARTIC_R2_MCU_COMMAND_ACCEPTED);

      // Read and print the ARGOS configuration
      if (success)
      {
        ARGOS_Configuration_Register configuration;
        myARTIC.readARGOSconfiguration(&configuration);
        myARTIC.printARGOSconfiguration(configuration);
      }

      // Set the ARGOS 2/3 TX frequency to 401.630 MHz
      // From AS3-SP-516-2098-CNES:
      // The transmission frequency for PTT/PMT-A2 platforms shall be set between 399.91 MHz to 401.68 MHz.
      // Due to frequency regulations, the frequency ranges [400.05 MHz to 401.0 MHz] and [401.2 MHz to 401.3 MHz] are forbidden for A2 transmissions.
      success &= myARTIC.setARGOS23TxFrequency(401.630);

      // Print the TX frequency
      if (success)
      {
        float tx23freq = myARTIC.getARGOS23TxFrequency();
        Serial.print(F("The ARGOS 2/3 TX Frequency is "));
        Serial.print(tx23freq, 3);
        Serial.println(F(" MHz."));
      }

      // Configure the Tx payload for ARGOS PTT-A2 using our platform ID and the latest lat/lon
      success &= myARTIC.setPayloadARGOS3LatLon(PLATFORM_ID, gnssLatitude, gnssLongitude);

      // Read the payload back again and print it
      if (success)
      {
        myARTIC.readTxPayload();
        myARTIC.printTxPayload();
        Serial.println();
      }

      if (success)
      {
        // ARTIC power-on and configuration was successful. Wait for next transmit.
        articIsOn = true;
        loop_step = wait_for_ARTIC_TX_to_start;
      }
      else
      {
        Serial.println("ARTIC R2 not detected. Please check the smôl stack-up and flexible circuits.");
        powerDownDuration = 6; // Power-down for 6 seconds and try again (the power-down duration is a useful diagnostic)
        loop_step = power_down;
      }
    }
    break;

    // ************************************************************************************************
    // The ARTIC is on, now wait until it is time to begin the transmit
    case wait_for_ARTIC_TX_to_start:
    {
      // Go round this case twice per second
      esp_sleep_enable_timer_wakeup(500000); //0.5 seconds
      esp_light_sleep_start();
      
      // Read the RTC
      struct timeval tv;
      gettimeofday(&tv, NULL);
      uint32_t epochNow = tv.tv_sec;

      // Calculate how many seconds remain until we power-on for the next transmit
      int32_t secsRemaining = (int32_t)nextTransmitStart - (int32_t)epochNow;

      // Count down in intervals of 100, then 10, then 1 second
      if (((secsRemaining >= 100) && (secsRemaining % 100 == 0)) ||
        ((secsRemaining < 100) && (secsRemaining % 10 == 0)) ||
        (secsRemaining < 10))
      {
        Serial.print(F("Next transmit will start in "));
        Serial.print(secsRemaining);
        Serial.print(F(" second"));
        if (secsRemaining != 1) // Attention to detail is everything... :-)
          Serial.println(F("s"));
        else
          Serial.println();
      }

      if (secsRemaining <= 0)
        loop_step = ARTIC_TX; // Move on

    }
    break;
    // ************************************************************************************************
    // Start the ARTIC in Transmit One Package And Go Idle mode
    case ARTIC_TX:
    {
      // Tell the ARTIC to do its thing!
      ARTIC_R2_MCU_Command_Result result = myARTIC.sendMCUinstruction(INST_TRANSMIT_ONE_PACKAGE_AND_GO_IDLE);
      if (result == ARTIC_R2_MCU_COMMAND_ACCEPTED)
      {
        loop_step = wait_for_ARTIC_TX_to_complete; // Move on        
      }
      else
      {
        Serial.println("sendMCUinstruction(INST_TRANSMIT_ONE_PACKAGE_AND_GO_IDLE) failed!");
        Serial.println();
        ARTIC_R2_Firmware_Status status;
        myARTIC.readStatusRegister(&status); // Read the ARTIC R2 status register
        Serial.println(F("ARTIC R2 Firmware Status:"));
        myARTIC.printFirmwareStatus(status); // Pretty-print the firmware status to Serial
        Serial.println();
        Serial.println(F("ARTIC_R2_MCU_Command_Result:"));
        myARTIC.printCommandResult(result); // Pretty-print the command result to Serial
        Serial.println();
        powerDownDuration = 8; // Power-down for 8 seconds and try again (the power-down duration is a useful diagnostic)
        loop_step = power_down;
      }
    }
    break;

    // ************************************************************************************************
    // Start the ARTIC in Transmit One Package And Go Idle mode
    case wait_for_ARTIC_TX_to_complete:
    {
      // Check the status every second
      esp_sleep_enable_timer_wakeup(1000000);
      esp_light_sleep_start();      

      // Read and print the ARTIC R2 status register
      ARTIC_R2_Firmware_Status status;
      myARTIC.readStatusRegister(&status); // Read the ARTIC R2 status register
      Serial.println(F("ARTIC R2 Firmware Status:"));
      myARTIC.printFirmwareStatus(status); // Pretty-print the firmware status to Serial

      if (status.STATUS_REGISTER_BITS.DSP2MCU_INT1) // Check the interrupt 1 flag. This will go high when TX is finished
      {
        Serial.println(F("INT1 pin is high. TX is finished (or MCU is in IDLE_STATE)!"));
      }

      if (status.STATUS_REGISTER_BITS.DSP2MCU_INT2) // Check the interrupt 2 flag. This will go high when if the message was invalid
      {
        Serial.println(F("INT2 pin is high. TX message was invalid! (Something really bad must have happened...)"));
      }

      Serial.println();

      // Read and print the instruction progress
      ARTIC_R2_MCU_Instruction_Progress progress;
      // checkMCUinstructionProgress will return true if the instruction is complete
      boolean instructionComplete = myARTIC.checkMCUinstructionProgress(&progress); // Check the instruction progress
      Serial.println(F("ARTIC R2 instruction progress:"));
      myARTIC.printInstructionProgress(progress); // Pretty-print the progress to Serial

      Serial.println();

      if (instructionComplete)
      {
        Serial.println(F("Transmission is complete!"));
        Serial.println();

        Serial.println(F("Clearing INT1."));
        Serial.println();

        // Clear INT1
        if (myARTIC.clearInterrupts(1) == false)
        {
          Serial.println("clearInterrupts failed!");
          powerDownDuration = 9; // Power-down for 9 seconds and try again (the power-down duration is a useful diagnostic)
          loop_step = power_down;
        }
        else
        {
          remainingTransmits--; // Decrement the remaining number of satellite transmits
          firstTransmit = false;
          if (remainingTransmits > 0) // Are we done?
          {
            nextTransmitTime += repetitionPeriod;
            nextTransmitStart = nextTransmitTime + random((-0.1 * repetitionPeriod), (0.1 * repetitionPeriod)); // Add the jitter
            nextTransmitStart -= tcxoWarmupTime; // Start the transmit early to compensate for the TCXO warmup time
            nextTransmitPowerOn = nextTransmitTime - articBootTimeMax - tcxoWarmupTime - (repetitionPeriod / 10); // Power-on early to compensate for the ARTIC boot time
            loop_step = wait_for_next_pass; // Wait for next transmit
          }
          else
          {
            Serial.println();
            Serial.println("All transmission attempts are complete!");
            Serial.println();
            Serial.println("Calculating next TX window...");
            Serial.println();
            loop_step = calculate_next_pass; // Do over...
          }
        }
      }
    }
    break;

    // ************************************************************************************************
    // Power-down
    case power_down:
    {
      digitalWrite(GNSS_PWR_EN_Pin, HIGH); // Disable power for the ZOE-M8Q
      digitalWrite(ARTIC_PWR_EN_Pin, LOW); // Disable power for the ARTIC

      myPowerBoard.setPowerdownDurationWDTInts(powerDownDuration); // Set the power-down duration (the I2C traffic this generates is a useful diagnostic)

      Serial.print(F("Power-down for "));
      Serial.print(powerDownDuration);
      Serial.println(F(" seconds..."));
      Serial.println();
      Serial.flush();
    
      myPowerBoard.powerDownNow(); // Thank you and good night...
      while (1)
        ; // Wait for power board to power back on again
    }
    break;
    
  }
}
