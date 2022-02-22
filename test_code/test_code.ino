/* File for working / test code (actively compiled and uploaded to Arduino).
 * Code from this file occasionally moved to "snipits" folder when substantive feature developed.
 * Logan Arkema, current date
*/


#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"
#include "TrainLine.h" 



char* ssid = SECRET_SSID;
const char* password = SECRET_PASS; 
const char* wmata_fingerprint = "D2 1C A6 D1 BE 10 18 B3 74 8D A2 F5 A2 DE AB 13 7D 07 63 BE"; //Expires 10/22/2022 
String wmata_host = "https://api.wmata.com";
String station_endpoint = "/TrainPositions/TrainPositions?contentType=json";

ESP8266WiFiMulti WiFiMulti;

//Function definition.
int checkEndOfLine(TrainLine &line, uint16_t* train_pos, uint8_t train_len);

void setup() {
  Serial.begin(9600); //NodeMCU ESP8266 runs on 9600 baud rate

  //Limit WiFi to station mode and connect to network defined in arduino_secrets.h
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);

  //Connect to WiFi
  while (WiFiMulti.run() != WL_CONNECTED){
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Wifi Connected");
  
}//END SETUP

// the loop function runs over and over again forever
void loop() {

  //Define client responsible for managing secure connection api.wmata.com
  WiFiClientSecure client;
  client.setFingerprint(wmata_fingerprint);
  client.setTimeout(15000);

  HTTPClient https;
  https.useHTTP10(true); //enables more efficient Json deserialization per https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/

  TrainLine redline = TrainLine();

  //Filters data from TrainPositions API to just get CircuitIds with Train
  StaticJsonDocument<48> train_pos_fiter;
  train_pos_fiter["TrainPositions"][0]["CircuitId"] = true;

  while (true){

    for(int i=redline.getLen()-1; i>=0; i--){

      Serial.println(redline.getState());
      uint16_t json_size = 2048;
      

      //Define URL for GET request and confirm correctness
      String endpoint = wmata_host + station_endpoint;
      
      //Connect and confirm HTTPS connection to api.wmata.com
      if (https.begin(client, endpoint)) {
        //If successful, add API key and request station data
        https.addHeader("api_key", SECRET_WMATA_API_KEY);
        int httpCode = https.GET();

        //Print out HTTP code and, if successful, parse data returned from API and update staion state
        if (httpCode > 0) {

          DynamicJsonDocument doc(json_size);
          DeserializationError error = deserializeJson(doc, https.getStream(), DeserializationOption::Filter(train_pos_fiter));

          //serializeJsonPretty(doc, Serial); //print out all outputs from deserialization
        

         //Iterate through all returned train positions, put the ones on target section of redline track into an array.
         uint16_t array_size = doc["TrainPositions"].size();
         uint16_t train_positions[10] = {};
         uint8_t k = 0;
         uint16_t j = 0;

         for(j=0; j<array_size; j++){ //TODO: replace with Iterators - https://arduinojson.org/v6/api/jsonarray/begin_end/
           const uint16_t circID = doc["TrainPositions"][j]["CircuitId"].as<unsigned int>();
           //Add train in test range to list
           if ( (circID >= redline.getStationCircuit(0) && circID <= redline.getStationCircuit(9)) || 
           (circID >= redline.getOppCID() && circID < (redline.getOppCID() + 2)) ){

             train_positions[k] = circID;
             k++;
           }
         }
         
          Serial.println("All trains in test range");
          for(int t=0; t<k; t++){
            Serial.printf("%d, ",train_positions[t]); /*Flawfinder: ignore */
          }
          Serial.println();

          if (redline.getLen() < 2){
            Serial.println("Setting initial state");
            redline.setInitialStations(train_positions, k);
            Serial.println(redline.getState());
          }

          //Put target station in local circuit
          uint16_t station_circuit = redline.getWaitingStationCircuit(i); //redline stores station indexes.

          //See if any of the current train positions are arriving, at, or just leaving destination station.
          //If so, update station state.
          for(int t=0; t<k; t++){
            if(train_positions[t] > (station_circuit - 3) && train_positions[t] < (station_circuit + 1) 
            && station_circuit != redline.getLastCID() ){

              Serial.printf("Station Circuit: %d\n", station_circuit); /*Flawfinder: ignore */
              Serial.printf("Train Circuit:   %d\n", train_positions[t]); /*Flawfinder: ignore */

              redline.arrived(i);

              break;
            }
          }

          checkEndOfLine(redline, train_positions, k);
          

        }
        else {
          Serial.println("GET Request failed");
          Serial.println(httpCode);
        }//end httpCode
        
      }//END https.begin

      else {
        Serial.println("HTTPS connection failed");
      }

    }//end for (per station check)
      delay(3000);
  }//end while(true) - all waiting stations loop
}//END LOOP

//Check if a train has reached the end of a line and, if so, whether or not to remove.
int checkEndOfLine(TrainLine &line, uint16_t* train_pos, uint8_t train_len){

  //Only enter if waiting for train at last station or train sitting at last station
  if(line[line.getLen()-1] == line.getTotalNumStations()-1 || line.getCyclesAtEnd() != 0){

    Serial.println("Checking last station");

    //If train reaches end of line, cycles_at_end is non-zero.
    if(line.getCyclesAtEnd() > 0){
      Serial.println("Incrementing cycle");
      if(line.incrementCyclesAtEnd() == 0){ //increment rolls over to 0 when max reached
        line.remove();
      }
      return 0;
    }

    uint16_t* possible_trains = new uint16_t[train_len];
    uint8_t possible_trains_len = 0;//index for possible_trains array

    //If train not at end of line, check if is (or isn't)
    for(int i=0; i<train_len; i++){
      //Only look at trains at or past 2nd to last station
      if(train_pos[i] >= line.getStationCircuit(line.getLen()-2) ){

        //If a train is between 2nd to last and last station, has not arrived yet. Return.
        if(train_pos[i] < (line.getLastCID() - 2) ){
          delete[] possible_trains;
          return -1;
        }

        Serial.printf("Possible Train: %d\n", train_pos[i]);
        possible_trains[possible_trains_len] = train_pos[i];
        possible_trains_len++;

      }//end if for at or past 2nd to last station
      
    }//end loop through active trains. Check all trains before checking 1st circuit in opp direction.

    //TODO: FIX THIS CODE
    for(uint8_t i=0; i < possible_trains_len; i++){
      if( ( (possible_trains[i] >= (line.getLastCID() - 3)) && (possible_trains[i] <= line.getLastCID()) ) ||
       possible_trains[i] == line.getOppCID() ){
        Serial.println("Last station reached");
        line.arrived(line.getLen()-1);
        line.incrementCyclesAtEnd();

        delete[] possible_trains;
        return 0;
      }
    }

  }
  return -1;
}