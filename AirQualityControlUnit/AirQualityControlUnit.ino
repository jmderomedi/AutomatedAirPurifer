#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

float bmeGasReading;
int relayControlPin = 15;

float hum_weighting = 0.25; // so hum effect is 25% of the total air quality score
float gas_weighting = 0.75; // so gas effect is 75% of the total air quality score

float hum_score, gas_score;
float gas_reference = 250000;
float hum_reference = 40;
int   getgasreference_count = 0;


//Adafruit_BME680 bme; // I2C
//Adafruit_BME680 bme(BME_CS); // hardware SPI
Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO,  BME_SCK);

void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println(("BME680 test"));

  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }

  pinMode(relayControlPin, OUTPUT);
  
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

}//END SETUP

void loop() {

  if (! bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    return;
  }

  //Calculate humidity contribution to IAQ index
  float current_humidity = bme.readHumidity();
  if (current_humidity >= 38 && current_humidity <= 42)
    hum_score = 0.25 * 100; // Humidity +/-5% around optimum
  else
  { //sub-optimal
    if (current_humidity < 38)
      hum_score = 0.25 / hum_reference * current_humidity * 100;
    else
    {
      hum_score = ((-0.25 / (100 - hum_reference) * current_humidity) + 0.416666) * 100;
    }
  }

  //Calculate gas contribution to IAQ index
  int gas_lower_limit = 5000;   // Bad air quality limit
  int gas_upper_limit = 50000;  // Good air quality limit
  if (gas_reference > gas_upper_limit) gas_reference = gas_upper_limit;
  if (gas_reference < gas_lower_limit) gas_reference = gas_lower_limit;
  gas_score = (0.75 / (gas_upper_limit - gas_lower_limit) * gas_reference - (gas_lower_limit * (0.75 / (gas_upper_limit - gas_lower_limit)))) * 100;

  //Combine results for the final IAQ index value (0-100% where 100% is good quality air)
  float air_quality_score = hum_score + gas_score;

  Serial.println("Air Quality = " + String(air_quality_score, 1) + "%");

  if ((getgasreference_count++) % 10 == 0) GetGasReference();
  Serial.println(CalculateIAQ(air_quality_score));

  //Turning on the relay if quality gets to bad
  air_quality_score = (100 - air_quality_score) * 5;
  Serial.println("Air Quality Score: " + String(air_quality_score, 1) + " After (100 - score) * 5");

  if (air_quality_score >= 55) {
    digitalWrite(relayControlPin, HIGH);
  } else {
    digitalWrite(relayControlPin, LOW);
  }

  Serial.println("------------------------------------------------");
  delay(2000);
}

/*
 * Reads the sensor 10 times over 1.5 sec interval and gets the average reading
 */
void GetGasReference() {
  Serial.println("Getting a new gas reference value");
  int readings = 10;
  for (int i = 0; i <= readings; i++) {
    gas_reference += bme.readGas();
  }
  gas_reference = gas_reference / readings;
}

/*
 * Calculates the IAQ score with the score calculated by the sensor
 * Prints out what the air quality is in terms of words
 */
String CalculateIAQ(float score) {
  String IAQ_text = "Air quality is ";
  score = (100 - score) * 5;
  if      (score >= 301)                  IAQ_text += "Hazardous ";
  else if (score >= 201 && score <= 300 ) IAQ_text += "Very Unhealthy";
  else if (score >= 176 && score <= 200 ) IAQ_text += "Unhealthy";
  else if (score >= 151 && score <= 175 ) IAQ_text += "Unhealthy for Sensitive Groups";
  else if (score >=  51 && score <= 150 ) IAQ_text += "Moderate";
  else if (score >=  00 && score <=  50 ) IAQ_text += "Good";
  return IAQ_text;
}
