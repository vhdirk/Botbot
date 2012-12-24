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

// emulate printf
#include <stdarg.h>
// include the time library, so we can keep track of the light duty cycle
#include <Time.h>


#define NUM_CONTAINERS 5
#define NUM_LIGHTS 5
#define NUM_HEATERS 5

//TODO: each moisture/light/heating unit should be a struct.

///////// Input/output definitions ///////// 
/// WATER
//const int numContainers = 5;
// moisture sensors (analog inputs)
const int moistureSensor[NUM_CONTAINERS] ={0,1,2,3,4};
// waterpump relays (digital, relay)
const int waterPump[NUM_CONTAINERS] = {2,3,4,5,6};

/// LIGHT
const int numLights = 5;
// light sensors (analog inputs)
const int lightSensor[NUM_LIGHTS] = {5,6,7,8,9}; 
// light switches (digital, relay)
const int lightSwitch[NUM_LIGHTS] = {15,16,17,18,19};

/// TEMPERATURE
const int numHeaters = 5;
// temperature sensors (analog inputs)
const int tempSensor[NUM_HEATERS] = {10,11,12,13,14};
// temperature switches (digital, led)
const int heatSwitch[NUM_HEATERS] = {20,21,22,23,24};



///////// Desired and measured values ///////// 

/// define variables to store moisture, light, and temperature values
int moistureValue[NUM_CONTAINERS];
int lightValue[NUM_LIGHTS];
int tempValue[NUM_HEATERS];

/// sensor thresholds
//moisture sensor uses a hysteresis
const float moistureThresholdLow[NUM_CONTAINERS] = {700, 700, 700, 700, 700};
const float moistureThresholdHigh[NUM_CONTAINERS] = {900, 900, 900, 900, 900};
int moistureLastMeasurement[NUM_CONTAINERS];

//decide how many hours of light your plants should get daily
const float lightThresholdLamps[NUM_LIGHTS] = {900,900,900,900,900};
const float lightThresholdSun[NUM_LIGHTS] = {1000,1000,1000,1000,1000};
const float lightProportionDesired[NUM_LIGHTS] = {14.0f/24.0f, 14.0f/24.0f, 14.0f/24.0f, 14.0f/24.0f, 14.0f/24.0f};
float lightTimeOn[NUM_LIGHTS] = {0,0,0,0,0};
float lightProportionAchieved[NUM_LIGHTS] = {0,0,0,0,0};
int lightLastMeasurement[NUM_LIGHTS];


const float tempThresholdLow[NUM_HEATERS] = {850, 850, 850, 850, 850};
const float tempThresholdHigh[NUM_HEATERS] = {1000, 1000, 1000, 1000, 1000};

// keep track of timing
time_t timeStart;
float timeElapsed;
float timeElapsedTotal;


float seconds_for_this_cycle;



// found here: http://playground.arduino.cc/Main/Printf
void serial_printf(char *fmt, ... ){
        char tmp[128]; // resulting string limited to 128 chars
        va_list args;
        va_start (args, fmt );
        vsnprintf(tmp, 128, fmt, args);
        va_end (args);
        Serial.print(tmp);
}


void setup()
{
    //open serial port
    Serial.begin(9600);
    
    //establish start time
    timeStart = now();
    timeElapsedTotal = 0;
    
    //install the water, light, and temperature output pins, and turn off outputs
    
    for (int i = 0; i < NUM_CONTAINERS; i++){
        pinMode (waterPump[i], OUTPUT);
        digitalWrite(waterPump[i], LOW);
    }
    
    for (int i = 0; i < NUM_LIGHTS; i++){
        pinMode (lightSwitch[i], OUTPUT);
        digitalWrite(lightSwitch[i], LOW);
        lightLastMeasurement[i] = timeStart;
    }
    
    for (int i = 0; i < NUM_HEATERS; i++){
        pinMode (heatSwitch[i], OUTPUT);
        digitalWrite(heatSwitch[i], LOW);
    }

}


void water_plant(int plant_nr)
{
    //read the value from the containers moisture-sensing probe
    moistureValue[plant_nr] = analogRead(moistureSensor[plant_nr]);
    serial_printf("moisture sensor #%i reads: %f\n", plant_nr,  moistureValue[plant_nr]);
    //delay(1000);
    
    //turn water on when soil is dry, and delay until soil is wet
    if (moistureValue[plant_nr] < moistureThresholdLow[plant_nr])
    {
        digitalWrite(waterPump[plant_nr], HIGH);
    }

    //TODO: need a routine here to stop watering the plants in case something goes wrong
    while (moistureValue[plant_nr] < moistureThresholdHigh[plant_nr])
    {
        delay(1000);
        moistureValue[plant_nr] = analogRead(moistureSensor[plant_nr]);
    }
    digitalWrite(waterPump[plant_nr], LOW);
    
}


void illuminate_plant(int plant_nr)
{
    // read the value from the photosensor, print it to screen, and wait a second
    lightValue[plant_nr] = analogRead(lightSensor[plant_nr]);
    serial_printf("light sensor #%i reads: %f \n", plant_nr,  lightValue[plant_nr]);
    
    // if the light is currently on, increase the timeOn value
    if (lightValue[plant_nr] > lightThresholdLamps[plant_nr]){
        lightTimeOn[plant_nr] += ( now() - lightLastMeasurement[plant_nr] );
    }
    
    // assuming there are windows: the sun is brighter than lamps, so if this threshold is exceeded, we don't need lamps anymore
    if (lightValue[plant_nr] > lightThresholdSun[plant_nr])
    {
        digitalWrite (lightSwitch[plant_nr], LOW);
    }

    // turn of the light if the desired light proportion has been achieved
    // TODO: perhaps this also needs a hysteresis or something of the sort
    if (lightProportionAchieved[plant_nr] > lightProportionDesired[plant_nr])
    {
        digitalWrite (lightSwitch[plant_nr], LOW);
    }

    //figure out what proportion of time lights have been on
    lightProportionAchieved[plant_nr] = lightTimeOn[plant_nr]/timeElapsedTotal;
    
    
    //turn lights on if light_val is less than 900 and plants have light for less than desired proportion of time, then wait 10 seconds
    if ( (lightValue[plant_nr] < lightThresholdLamps[plant_nr]) && (lightProportionAchieved[plant_nr] < lightProportionDesired[plant_nr]) )
    {
        digitalWrite(lightSwitch[plant_nr], HIGH);
        //delay(10000);
    }

    
    serial_printf("Desired illumination proportion #%i: %f\n", plant_nr,  lightProportionDesired[plant_nr]);
    serial_printf("Achieved illumination proportion #%i: %f\n", plant_nr,  lightProportionAchieved[plant_nr]);

    //delay(1000);
}


void warm_plant(int plant_nr)
{
    // read the value from the photosensor, print it to screen, and wait a second
    tempValue[plant_nr] = analogRead(tempSensor[plant_nr]);
    serial_printf("temperature sensor #%i reads: %f\n", plant_nr,  tempValue[plant_nr]);


    //turn on the heat switch if the temperature dropped below the lowest threshold
    if (tempValue[plant_nr]  < tempThresholdLow[plant_nr])
    {
        digitalWrite(heatSwitch[plant_nr], HIGH);
    }
    
    //turn of the heating it the temperature exceeds the highest threshold
    if (tempValue[plant_nr]  > tempThresholdHigh[plant_nr])
    {
        digitalWrite(heatSwitch[plant_nr], LOW);
    }

}



void loop()
{
    serial_printf("testing printf %i, yay", 50);

    for (int i = 0; i < NUM_CONTAINERS; i++){
        
        water_plant(i);
    }
    
    for (int i = 0; i < NUM_LIGHTS; i++){
        illuminate_plant(i);
    }
    
    for (int i = 0; i < NUM_HEATERS; i++){
        warm_plant(i);
    }

}
