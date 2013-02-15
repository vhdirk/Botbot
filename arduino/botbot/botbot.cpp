///////////////////////////////////////////////////////////////////////////////
//
// Botbot (Botanic robot): Automatic plant watering system for Seeker HS2
// More information: [http://www.z33.be/en/projects/space-odyssey-20]
//
// Author: Dirk Van Haerenborgh
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//
///////////////////////////////////////////////////////////////////////////////
//
// Arduino mega memo:
// * PWM: 2 to 13 and 44 to 46. Provide 8-bit PWM output with the analogWrite() function.
//
///////////////////////////////////////////////////////////////////////////////


#include <Arduino.h>
#include <Ethernet.h>
#include <SPI.h>

// emulate printf
#include <stdarg.h>
// include the time library, so we can keep track of the light duty cycle
#include <Time.h>

#include <aJSON.h>


#define ENABLE_POST

#define NUM_CONTAINERS_TOTAL 6
#define NUM_CONTAINERS_USED 2


//ugh, global variables. Shoot me now.

///////// Input/output definitions ///////// 
/// WATER
// moisture sensors (analog inputs)
const int moistureSensor[NUM_CONTAINERS_TOTAL] = {8,9,10,11,12,13};

// power to the moisture sensors
const int moistureOutTop[NUM_CONTAINERS_TOTAL] = {38,40,42,44,46,48};
const int moistureOutBottom[NUM_CONTAINERS_TOTAL] = {39,41,43,45,47,49};

// waterpump relays (digital, relay)
const int waterPump[NUM_CONTAINERS_TOTAL] = {23,25,27,29,31,33};



/*
  Cube Server IP
*/
byte server[] = { 192, 168, 2, 30 };

/*
  Local IP settings
*/
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0xA3, 0x3D };
byte ip[] = { 192, 168, 2, 35 };
byte submask[] = { 255, 255, 255, 0 };
byte gateway[] = { 192, 168, 2, 254 };
byte dnss[] = { 192, 168, 2, 254 };

EthernetClient client;


///////// Desired and measured values ///////// 

// store moisture values
int moistureValue[NUM_CONTAINERS_TOTAL];

// remember if we watered the last iteration
int waterLastIter[NUM_CONTAINERS_TOTAL] = {0,0,0,0,0,0};

/// sensor thresholds
// moisture sensor uses a hysteresis

// 700 - 800 seems like an ideal moist level.
const float moistureThresholdLow[NUM_CONTAINERS_TOTAL] = {400, 400, 400, 400, 400};
const float moistureThresholdHigh[NUM_CONTAINERS_TOTAL] = {900, 900, 900, 900, 900};
int moistureLastMeasurement[NUM_CONTAINERS_TOTAL];


// keep track of timing
time_t timeStart;
float timeElapsed;
float timeElapsedTotal;


float seconds_for_this_cycle;

/*!
 * \brief serial_printf C-like printf function
 * found here: http://playground.arduino.cc/Main/Printf
 */
void serial_printf(char *fmt, ... ){
  char tmp[128]; // resulting string limited to 128 chars
  va_list args;
  va_start (args, fmt );
  vsnprintf(tmp, 128, fmt, args);
  va_end (args);
  Serial.print(tmp);
}

/*!
 * \brief setup in/outputs
 */
void setup()
{

  //open serial port
  Serial.begin(9600);

  //establish start time
  timeStart = now();
  timeElapsedTotal = 0;

  //install the water, light, and temperature output pins, and turn off outputs
  for (int i = 0; i < NUM_CONTAINERS_TOTAL; i++){
    pinMode (waterPump[i], OUTPUT);
    pinMode (moistureOutTop[i], OUTPUT);
    pinMode (moistureOutBottom[i], OUTPUT);

    digitalWrite(waterPump[i], LOW);
    digitalWrite(moistureOutTop[i], LOW);
    digitalWrite(moistureOutBottom[i], LOW);
  }

  //TODO: define led output
  pinMode(LED_BUILTIN, OUTPUT); //LED_BUILTIN = 13
  digitalWrite(LED_BUILTIN, LOW);

#ifdef ENABLE_POST

//  Ethernet.begin(mac, ip); //, dnss, gateway, submask);

//  Serial.print("My IP address: ");
//  Serial.println(Ethernet.localIP());

//  while(!client){
//    Serial.print(".");
//  }

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;) Serial.print(".");
      ;
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println("connecting...");


#endif //ENABLE_POST



}


/*
  Drives a current though two digital pins
  and reads the resistance through the
  analogue pin
*/
int measure_moisture(int plant_nr)
{
  // drive a current through the divider in one direction
  digitalWrite(moistureOutTop[plant_nr],HIGH);
  digitalWrite(moistureOutBottom[plant_nr],LOW);
  delay(500);

  // take a reading
  int reading = analogRead(moistureSensor[plant_nr]);

  delay(10);

  // reverse the current to prevent crust forming on probes
  digitalWrite(moistureOutTop[plant_nr],LOW);
  digitalWrite(moistureOutBottom[plant_nr],HIGH);
  delay(510);

  // stop the current
  digitalWrite(moistureOutBottom[plant_nr],LOW);

  return reading;
}




/*!
 * \brief water_plant
 * \param plant_nr
 *
 */
void water_plant(int plant_nr)
{
  //read the value from the containers moisture-sensing probe
  moistureValue[plant_nr] = measure_moisture(plant_nr);
  serial_printf("moisture sensor #%i reads: %i\n", plant_nr,  moistureValue[plant_nr]);

  //NOTE: if the measuring probe reading produces some weird results,
  //      we do not want to keep pumping water endlessly
  //      Hence, we will water the plant for a total maximum of 10 seconds.
  //      Additionally, we will light the warning led if we need to water 2 consecutive iterations.
  //      That plant will receive no more water until reset.

  bool needs_water = moistureValue[plant_nr] < moistureThresholdLow[plant_nr];
  if (!needs_water){
    // plant does not need water. That's cool.
    waterLastIter[plant_nr] = 0;
  }
  else if (waterLastIter[plant_nr]>=1)
  {
    // plant does need water, but we already watered last iteration.
    // Since we are using a hysteresis, something odd is going on.
    // So, light the warning led, and stop watering the plant until someone
    // pressed the reset button
    digitalWrite(LED_BUILTIN, HIGH);
    waterLastIter[plant_nr] += 1; //increment, so we only have '1' when we actually did water the plant

    // if the led is lit, check all sensors/pumps and reset the arduino board
    // TODO: an interrupt would be soo much nicer

  }
  else
  {
    serial_printf("plant #%i needs water\n", plant_nr);

    //turn water on when soil is dry, and delay until soil is wet
    digitalWrite(waterPump[plant_nr], HIGH);

    int max_iters=10; //water for maximum 10 seconds
    int num_iters=0;
    while (( moistureValue[plant_nr] < moistureThresholdHigh[plant_nr]) && (num_iters < max_iters))
    {
      // reading the moisture itself takes 1 a 2 seconds (depending on what delays I entered there)
      // so we do not need an additional delay
      moistureValue[plant_nr] = measure_moisture(plant_nr);
      ++num_iters;
    }
    // turn of the pump
    digitalWrite(waterPump[plant_nr], LOW);

    // remember that we watered.
    waterLastIter[plant_nr] += 1;
  }

}


#ifdef ENABLE_POST


/*!
 * \brief post_to_server
 *
 * Posts a JSON document to the cube server (see http://square.github.com/cube/)
 *
 * The JSON document describes the soil moisture and whether the plant was
 * watered.
 *
 * [
 *  {
 *   "type": "plant_<nr>",
 *   "data": {
 *     "moisture": 600,
 *     "watered": 0
 *    }
 *  },
 *   "type": "plant_<nr+1>",
 *   "data": {
 *     "moisture": 600,
 *     "watered": 0
 *    }
 *  }
 * ]
 *
 * Using int rather than boolean for 'watered'
 * as cube metric querying is limited with booleans
 *
 */
void post_to_server()
{
  //TODO: this will hang if the server is unreachable
  // If it hangs here, plants will not receive water any longer
  // This could be bad. However, we do not want to flood them either


  while(!client.connected())
  {
    client.stop();
    Serial.println("Connecting...");
    if (client.connect(server,1080)) {
      Serial.println("Connected");
    } else {
      Serial.println("Connection failed");
      delay(5000);
    }
  }

  //TODO: who is repsonsible for freeing these pointers?
  aJsonObject* rootJson = aJson.createArray();

  for (int i = 0; i < NUM_CONTAINERS_USED; i++)
  {
    aJsonObject* event = aJson.createObject();
    aJson.addItemToArray(rootJson, event);

    char event_name[8];
    sprintf(event_name, "plant_%i", i);

    serial_printf("sending event for: '%s'', moisture: %i\n", event_name, moistureValue[i]);


    aJson.addStringToObject(event, "type", event_name);
    aJsonObject* data = aJson.createObject();
    aJson.addItemToObject(event, "data", data);
    int rnd= random(0, 1023);
    aJson.addNumberToObject(data, "moisture", moistureValue[i]);
    aJson.addNumberToObject(data, "watered", int(waterLastIter[i]==1) );
  }

  char* jsonstr = aJson.print(rootJson);

  serial_printf("sending: %s\n", jsonstr);


  client.println("POST /1.0/event/put");
  client.println(" HTTP/1.1");
  client.println("Connection: keep-alive");
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println("Content-Length: " + String(strlen(jsonstr)));
  client.println("User-Agent: arduino-ethernet");        // ethernet related stuff
  //client.println("cube-password: test");
  client.println();
  client.println(jsonstr);

  //free the char*
  if (jsonstr != NULL){
    free(jsonstr);
    jsonstr = NULL;
  }

  //delete the json tree
  aJson.deleteItem(rootJson);

}

#endif //ENABLE_POST



/*!
 * \brief main arduino processing loop
 */
void loop()
{
  Serial.print(".\n");
  for (int i = 0; i < NUM_CONTAINERS_USED; i++)
  {
    water_plant(i);
    delay(1000);
  }


#ifdef ENABLE_POST
  post_to_server();
#endif //ENABLE_POST

  //TODO: delay for 10 seconds or more
  //delay(1000);

}



