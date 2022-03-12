/*
* File for miscellaneous functions to test separate from main Arduino code
* Also useful for keeping bulky functions out of main file.
*
*/

#include "TrainLine.h"

//Check if a train has reached the end of a line and, if so, whether or not to remove.
//TODO: APPLY OPPOSITE DIRECTION (Negative Circuit traversal) LOGIC
int checkEndOfLine(TrainLine &line, uint16_t* train_pos, uint8_t train_len, bool dir){

  //Only enter if waiting for train at last station or train sitting at last station
  if(line.at(line.getLen(dir)-1, dir) == line.getTotalNumStations()-1 || line.getCyclesAtEnd(dir) != 0){

    //Serial.println("Checking last station");

    //If train reaches end of line, cycles_at_end is non-zero.
    if(line.getCyclesAtEnd(dir) > 0){
      Serial.println("Incrementing cycle");
      if(line.incrementCyclesAtEnd(dir) == 0){ //increment rolls over to 0 when max reached
        line.remove(dir);
      }
      return 0;
    }

    uint16_t* possible_trains = new uint16_t[train_len];
    uint8_t possible_trains_len = 0;//index for possible_trains array

    //If train not at end of line, check if is (or isn't)
    for(int i=0; i<train_len; i++){
      //Only look at trains arriving at or past 2nd to last station
      if(train_pos[i] >= line.getStationCircuit(line.getTotalNumStations()-2, dir)-2 ){
        

        //If a train is between 2nd to last and last station, has not arrived yet. Return.
        if(train_pos[i] < (line.getLastCID(0) - 2) ){
          //Serial.println("Train en route to last. Not arrived yet.");
          //Serial.println(String(train_pos[i]));

          delete[] possible_trains;
          return -1;
        }

        //Serial.printf("Possible Train: %d\n", train_pos[i]);

        possible_trains[possible_trains_len] = train_pos[i];
        possible_trains_len++;

      }//end if for at or past 2nd to last station
      
    }//end loop through active trains. Check all trains before checking 1st circuit in opp direction.

    //TODO: FIX THIS CODE
    for(uint8_t i=0; i < possible_trains_len; i++){
      Serial.println(String(possible_trains[i]));

      if( ( (possible_trains[i] >= (line.getLastCID(0) - 3)) && (possible_trains[i] <= line.getLastCID(0)) ) ||
       possible_trains[i] == line.getOppCID(0) ){
        Serial.println("Last station reached");
        line.arrived(line.getLen(0)-1, 0);
        line.incrementCyclesAtEnd(0);

        delete[] possible_trains;
        return 0;
      }
    }

    delete[] possible_trains;
  }
  return -1;
}