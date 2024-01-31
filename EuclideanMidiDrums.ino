/*

  Euclidean Midi Drums
  Copyright (c) 2024 blueprint@poetaster.de (Mark Washeim)
  some parts Copyright (c) 2020 diyelectromusic (Kevin)

  GPLv3 See LICENSE distributed with this file.

  Much of the code and inspiration is from Kevin
  Simple DIY Electronic Music Projects - https://diyelectromusic.wordpress.com

  The euclidean rythm code is from https://bastl-instruments.com/ the one chip modules
  https://github.com/bastl-instruments/one-chip-modules

  This uses a generic VS1003 or VS1053 MP3 Player board utilising
  VS10xx software patching to provide a robust "real time MIDI" mode
  with MIDI access provided over the SPI bus.

*/

#include <SPI.h>
#include <MIDI.h>
#include "vs10xx_uc.h" // From VLSI website: http://www.vlsi.fi/en/support/software/microcontrollersoftware.html

//#include "euclid.h"

// the pattern generator
//euclid euc;
#define NUMBER_OF_STEPS 32

// Euclidean sequences
bool euclidianPattern[NUMBER_OF_STEPS + 1];
uint8_t stepCounter;
uint8_t numberOfSteps;
uint8_t numberOfFills = 6;
uint8_t numberOfrepeats = 0;

//#define TEST 1

// Uncomment to perform a test play of the instruments and drums on power up
//#define SOUND_CHECK 1

// Uncomment this if you have a test LED
// Note: The LED_BUILTIN for the Uno is on pin 13,
//       which is used below for the VS1003 link.
//#define MIDI_LED1 4
//#define MIDI_LED2 5


// button pins digita
#define B2_PIN 2
#define B3_PIN 3


// This code supports several variants of the VS10xx based shields.
// Choose the apprppriate one here (and comment out the others).
//
#define VS1053_MP3_SHIELD 1
//#define VS1003_MODULE 1

#ifdef VS1003_MODULE
extern "C" {
#include "rtmidi1003b.h"
#include "vs1003inst.h"
}
#endif
#ifdef VS1053_MP3_SHIELD
extern "C" {
#include "rtmidi1053b.h"
}
#endif

// Use binary to say which MIDI channels this should respond to.
// Every "1" here enables that channel. Set all bits for all channels.
// Make sure the bit for channel 10 is set if you want drums.
//
//                               16  12  8   4  1
//                               |   |   |   |  |
uint16_t MIDI_CHANNEL_FILTER = 0b1111111111111111;

// Comment out any of these if not in use
//#define POT_MIDI  A0 // MIDI control
//#define POT_VOL   A3 // Volume control

// POTs to control which set of drums and tempo
#define POT_TEMPO   A0
#define POT_FILLS   A1
#define POT_KIT     A2
#define POT_REPEATS A3

// modulation pins analog
#define M1P 5
#define M2P 6
#define M3P 7


// Channel to link to the potentiometer (1 to 16)
#define POT_MIDI_CHANNEL 10

#ifdef VS1053_MP3_SHIELD
// VS1053 Shield pin definitions
#define VS_XCS    9 // 6 Control Chip Select Pin (for accessing SPI Control/Status registers)
#define VS_XDCS   6 // 7 Data Chip Select / BSYNC Pin
#define VS_DREQ   7 // 9 Data Request Pin: Player asks for more data
#define VS_RESET  8 // 8 Reset is active low
#endif
#ifdef VS1003_MODULE
// VS1003 Module pin definitions
#define VS_XCS    9 //8 // Control Chip Select Pin (for accessing SPI Control/Status registers)
#define VS_XDCS   6 // 9 // Data Chip Select / BSYNC Pin
#define VS_DREQ   7 // Data Request Pin: Player asks for more data
#define VS_RESET  8 // 10 // Reset is active low
#endif
// VS10xx SPI pin connections (both boards)
// Provided here for info only - not used in the sketch as the SPI library handles this
#define VS_MOSI   11
#define VS_MISO   12
#define VS_SCK    13
#define VS_SS     10

// Optional - use Digital IO as the power pins
//#define VS_VCC    6
//#define VS_GND    5

// There are three selectable sound banks on the VS1053
// These can be selected using the MIDI command 0xBn 0x00 bank
#define DEFAULT_SOUND_BANK 0x00  // General MIDI 1 sound bank
#define DRUM_SOUND_BANK    0x78  // Drums
#define ISNTR_SOUND_BANK   0x79  // General MIDI 2 sound bank 0x7f

// List of instruments to send to any configured MIDI channels.
// Can use any GM MIDI voice numbers here (1 to 128), or more specific definitions
// (for example as found in vs1003inst.h for the VS1003).
//
// 0 means "ignore"
//
byte preset_instruments[16] = {
  /* 01 */  1,
  /* 02 */  9,
  /* 03 */  17,
  /* 04 */  25,
  /* 05 */  30,
  /* 06 */  33,
  /* 07 */  41,
  /* 08 */  49,
  /* 09 */  57,
  /* 10 */  0,  // Channel 10 will be ignored later as that is percussion anyway.
  /* 11 */  65,
  /* 12 */  73,
  /* 13 */  81,
  /* 14 */  89,
  /* 15 */  113,
  /* 16 */  48
};

// This is required to set up the MIDI library.
// The default MIDI setup uses the built-in serial port.
MIDI_CREATE_DEFAULT_INSTANCE();

// Defaults, but will be overridden by POT settings if enabled
uint16_t volume = 0;
uint16_t tempo  = 120;
byte instrument;


#ifdef TEST
char teststr[32];
#endif

#define KIT 8

#define VS1003_D_BASS     35   // 35 36 86 87
#define VS1003_D_SNARE    38   // 38 40
#define VS1003_D_HHC      42   // 42 44 71 80 // good
#define VS1003_D_HHO      46   // 46 55 58 72 74 81 // long siss
#define VS1003_D_HITOM    50   // 48 50
#define VS1003_D_LOTOM    45   // 41 43 45 47
#define VS1003_D_CRASH    57   // 49 57
#define VS1003_D_RIDE     51   // 34 51 52 53 59
#define VS1003_D_TAMB     54   // 39 54
#define VS1003_D_HICONGA  62   // 60 62 65 78
#define VS1003_D_LOCONGA  64   // 61 63 64 66 79
#define VS1003_D_MARACAS  70   // 27 29 30 69 70 73 82 
#define VS1003_D_CLAVES   75   // 28 31 32 33 37 56 67 68 75 76 77 83 84 85

int kits[KIT][13] = {
  { 42, 36, 46, 42, 37, 36, 42, 46, 46, 35, 38, 46, 38 }, // clicks, side stick, snare bass
  { 35, 40, 42, 46, 35, 40, 42, 46, 42, 38, 64, 37, 42 }, // not bad
  { 36, 40, 36, 42, 42, 40, 40, 70, 46, 36, 75, 70, 37 }, // not bad
  { 35, 38, 42, 40, 35, 42, 70, 42, 46, 70, 46, 35, 42 }, // little more on the snare side/ HH, lo & hi conga, and maracas
  { 35, 38, 42, 40, 36, 46, 36, 42, 46, 38, 37, 61, 42 }, // little more on the snare side/ HH, lo conga, one clave hit
  { 46, 38, 40, 42, 46, 70, 42, 46, 44, 70, 46, 37, 64 }, // clicks snare side stick maracas
  { 44, 76, 42, 44, 77, 42, 44, 77, 44, 46, 44, 46, 85 }, // clicks, hh, castenets
  { 27, 28, 29, 30, 31, 32, 33, 34, 69, 76, 77, 78, 79 },
}; // Bass, Snare, HHO, HHC

int kit = 0;




void setup() {

  // initialize the pushbuttons on rampart as an input:
  pinMode(B2_PIN, INPUT);
  pinMode(B3_PIN, INPUT);

  Serial.begin(9600);

#ifdef Test
  Serial.println ("Initialising VS10xx");
#endif // TEST

  // put your setup code here, to run once:
  initialiseVS10xx();


  // This listens to all MIDI channels
  // They will be filtered out later...
#ifndef TEST
  MIDI.begin(MIDI_CHANNEL_OMNI);
#endif // TEST

  delay(200);

#ifdef SOUND_CHECK
  // A test "output" to see if the VS0xx is working ok
  for ( byte i = 2; i < 127; i++) {

    int rr = random(13);
    int note = kits[kit][rr];
    talkMIDI (0x99, i, 127);
    Serial.println (i);
    delay (100);
    talkMIDI (0x89, note, 0);
    delay (500);
  }
#endif // SOUND_CHECK

  // Configure the instruments for all required MIDI channels.
  // Even though MIDI channels are 1 to 16, all the code here
  // is working on 0 to 15 (bitshifts, index into array, MIDI command).
  //
  // Also, instrument numbers in the MIDI spec are 1 to 128,
  // but need to be converted to 0 to 127 here.
  //
  for (byte ch = 0; ch < 16; ch++) {
    if (ch != 9) {  // Ignore channel 10 (drums)
      if (preset_instruments[ch] != 0) {
        uint16_t ch_filter = 1 << ch;
        if (MIDI_CHANNEL_FILTER & ch_filter) {
          talkMIDI(0xC0 | ch, preset_instruments[ch] - 1, 0);
        }
      }
    }
  }
  // try set second kit.
  //talkMIDI(0x00,0xb0,0x7f);
  // For some reason, the last program change isn't registered
  // without an extra "dummy" read here.
  talkMIDI(0x80, 0, 0);
  
  // Set these invalid to trigger a read of the pots
  // (if being used) first time through.

  // Set these invalid to trigger a read of the pots
  // (if being used) first time through.
#ifdef POT_TEMPO
  tempo  = -1;
#endif
#ifdef POT_VOL
  volume = -1;
#endif


  /**** set up Euclidean sequences ****/
  generateSequence(8,31);
 // startBeat();

  /*
    #ifdef VS_VCC
    pinMode(VS_VCC, OUTPUT);
    digitalWrite(VS_VCC, HIGH);
    #endif // VS_VCC
    #ifdef VS_GND
    pinMode(VS_GND, OUTPUT);
    digitalWrite(VS_GND, LOW);
    #endif // VS_GND
    #ifdef MIDI_LED1
    pinMode(MIDI_LED1, OUTPUT);
    // flash the LED on startup
    for (int i=0; i<4; i++) {
      digitalWrite(MIDI_LED1, HIGH);
      delay(100);
      digitalWrite(MIDI_LED1, LOW);
      delay(100);
    }
    #endif // MIDI_LED1
    #ifdef MIDI_LED2
    pinMode(MIDI_LED2, OUTPUT);
    // flash the LED on startup
    for (int i=0; i<4; i++) {
      digitalWrite(MIDI_LED2, HIGH);
      delay(100);
      digitalWrite(MIDI_LED2, LOW);
      delay(100);
    }
    #endif // MIDI_LED2
  */
}

// Globals, clean up!
uint8_t steps, fills, lastSteps, lastFills, repeats ;
uint8_t currentRepeat = 0;
bool bState1, bState2;
bool swing = 0;
uint8_t clkCounter = 0;
uint8_t instrs;
uint8_t timings;
int loopstate;


// time keeping global variables much from drum kid
float nextPulseTime = 0.0; // the time, in milliseconds, of the next pulse
float msPerPulse = 20.8333; // time for one "pulse" (there are 24 pulses per quarter note, as defined in MIDI spec)
byte pulseNum = 0; // 0 to 23 (24ppqn, pulses per quarter note)
unsigned int stepNum = 0; // 0 to 192 (max two bars of 8 beats, aka 192 pulses)
unsigned int numSteps = 32; // number of steps used - dependent on time signature
bool syncReceived = false; // has a sync/clock signal been received? (IMPROVE THIS LATER)

/*
 *storedValues[BEAT] = 27;
  storedValues[TIME_SIGNATURE] = 64; // equates to 4/4
  storedValues[SWING] = 0;
  storedValues[TEMPO] = 110; // actually equates to 120BPM
  if(!beatPlaying) {
    // don't randomise time signature and tempo unless stopped
    storedValues[TIME_SIGNATURE] = 64; // equates to 4/4
    if(rand(0,3)==0) storedValues[TIME_SIGNATURE] = rand(0,256);
    storedValues[TEMPO] = rand(70,160);
    if(rand(0,4)==0) storedValues[TEMPO] = rand(0,256);
  }
  storedValues[SWING] = rand(0,256);
 */
unsigned long nexttick;

// tempo variables
float tapTimes[4];
byte nextTapIndex = 0;
byte paramTimeSignature = 64; // 1 to 13 (3/4, 4/4, 5/4, 6/4, 7/4)
float paramTempo = 80; 
byte paramSwing = 0; // 0 to 2 (0=straight, 1=approx quintuplet swing, 2=triplet swing)

int syncTicks = 0;
unsigned long synctick;
unsigned long synctick2;
unsigned long sync;
// main loop

void loop() {

#ifdef TEST

  //Serial.println ( getStepNumber() );

#endif

  /* Drum kid
  
  msPerPulse = 2500.0 / tempo ; // paramTempo;

  // perform pulse actions inside loop - stop beat if everything gets super overloaded
  byte numLoops = 0;
  while(!syncReceived && beatPlaying && millis()>=nextPulseTime) {
    if(numLoops<5) doPulseActions();
    nextPulseTime = nextPulseTime + msPerPulse;
    if(numLoops>=100) beatPlaying = false;
    numLoops ++;
  }
  /*
  // Only play the note in the pattern if we've met the criteria for
  // the next "tick" happening.
  */
  msPerPulse = 2500.0 / tempo ; // 20.833 ms per beat at 4/4 120BPM
  
  unsigned long timenow = millis();
  if (timenow >= nexttick) {
      //nexttick = sync + millis();
      // we use a multiplier of parts of fill in 32 steps. 
      // this should be adjustable 
      
      nexttick = millis() + (unsigned long)(1000 / (tempo / 60) )/ 24;
      //nexttick = nexttick + msPerPulse;
      
    //Serial.println(nexttick);
    // check if we've hit the end of a 32 step pattern
    // if so, new pattern. reset counter.
    if ( clkCounter == 32) {
      if (currentRepeat == repeats) {
        generateSequence(numberOfFills, 31);
        clkCounter = 0;
        currentRepeat = 0;
        Serial.println("new pattern");

      }
    }
    // set next repeat counter on pattern end
    if (clkCounter == 32) {
      currentRepeat++;
      Serial.print("incr repeat: ");
      Serial.println(currentRepeat);
    }

    // update / play sound if currentstep is on
    if (getCurrentStep() ) {
      //Serial.println(getStepNumber());
      int rr = random(13); // should use length of [kit]
      int note = kits[kit][rr];
      //Serial.println ( note);
      // we're addding a bit of randomness to velocity
      int vel = 127 - random(20);
      if (note == 75 || note == 57) {
        vel = 90 ;
      }
      talkMIDI (0x99, note, vel);

      //swing a bit but not too much
      if (swing) {
        delay (random(30));
        //Serial.println("swinging");
      }

      //talkMIDI (0x89, note, 0);
      //delay (50);
    }
    // generate the current step 0/1, increment counter
    doStep();
    clkCounter++;
    
#ifdef TEST
/*
    Serial.print ("KIT: ");
    Serial.println ( kit);

    Serial.print ("REPEATS: ");
    Serial.println ( repeats);
    //Serial.print ("CURREPEAT: ");
    //Serial.println ( currentRepeat);
    //Serial.println ("");
    Serial.print ("TEMPO: ");
    Serial.println ( tempo);
    Serial.print ("FILLS: ");
    Serial.println ( numberOfFills);
*/
#endif
  }

  if (loopstate == 0) {
    // Read the potentiometer for a value between 0 and 127
    // to use to select the instrument in the general MIDI bank
    // for POT_MIDI_CHANNEL (if POT_MIDI is defined).

#ifdef POT_KIT
    int pot0 = map(analogRead(POT_KIT), 0, 1023, 0, KIT);
    if (pot0 != kit) {
      kit = pot0;
      Serial.print ("KIT: ");
      Serial.println ( kit);

    }
#endif // POT_KIT

#ifdef POT_FILLS
    int pot3 = map(analogRead(POT_FILLS), 0, 1023, 1, 16);
    if (pot3 != numberOfFills) {
      numberOfFills = pot3;  // Number of fills 4-16
      // reset repeats
      Serial.println(numberOfFills);
      currentRepeat = 0;
      clkCounter = 0;
    }
#endif // POT_FILLS
  }

  else if (loopstate == 1) {

#ifdef POT_TEMPO
    int pot1 = map(analogRead(POT_TEMPO), 0, 1023, 40, 240);
    if (pot1 != tempo) {
      tempo = pot1;  // Tempo range is 20 to 275.
      currentRepeat = 0;
      clkCounter = 0;
      Serial.print ("Tempo: ");
      Serial.println ( tempo);

    }
#endif // POT_TEMPO

  }
  else if (loopstate == 2) {

#ifdef POT_REPEATS
    int pot2 = map(analogRead(POT_REPEATS), 0, 1023, 0, 33);
    if (pot2 != repeats) {
      repeats = pot2;  // 0 to 32 repeats
      currentRepeat = 0;
      clkCounter = 0;
      Serial.print ("Repeats: ");
      Serial.println ( repeats);
    }
#endif // POT_REPEATS

  }
  loopstate++;
  if (loopstate > 2) loopstate = 0;


  // this is TOGGLE logic. move to debounce
  // button handling for number of steps
  static int previous1, previous2;
  int current1 = digitalRead(B2_PIN);
  int current2 = digitalRead(B3_PIN);

  if (previous1 == LOW && current1 == HIGH) {
    if (bState1 == 1) {
      bState1 = 0;
      swing = 0;
    } else {
      bState1 = 1;
      swing = 1;
    }
    //Serial.println("But1");
    //Serial.println(bState1);
  }
  previous1 = current1;
  if ( bState1 == 0 ) {
  } else if ( bState1 == 1 ) {
  }

  if (previous2 == LOW && current2 == HIGH) {
    if (bState2 == 1) {
      bState2 = 0;
    } else {
      bState2 = 1;
    }
    Serial.println("But2");
    Serial.println(bState2);
  }
  previous2 = current2;
  if ( bState2 == 0 ) {
    //updateWavePacket();
  } else if ( bState2 == 1 ) {
    //updateFM();

  }
  
  /* tap tempo logic
      if(buttons[5].fell()) {
        tapTimes[nextTapIndex] = (float) millis();

        float firstTime;
        float timeTally = 0.0;
        byte validTimes = 0;
        bool keepChecking = true;
        for(i=0;i<NUM_TAP_TIMES-1 && keepChecking;i++) {
          byte thisTapIndex = (nextTapIndex + NUM_TAP_TIMES - i) % NUM_TAP_TIMES;
          byte lastTapIndex = (nextTapIndex + NUM_TAP_TIMES - i - 1) % NUM_TAP_TIMES;

          float thisTime = tapTimes[thisTapIndex] - tapTimes[lastTapIndex];
          if(i==0) firstTime = thisTime;
          float timeCompare = firstTime/thisTime;
          if(tapTimes[lastTapIndex] > 0.0 && timeCompare > 0.8 && timeCompare < 1.2) {
            timeTally += thisTime;
            validTimes ++;
          } else {
            keepChecking = false;
          }
        }
        if(validTimes>=2) {
          float newPulseLength = (timeTally / validTimes) / 24;
          paramTempo = 2500.0 / newPulseLength;
          storedValues[TEMPO] = tempoToByte(paramTempo);
        }
        nextTapIndex = (nextTapIndex + 1) % NUM_TAP_TIMES;
        
        if(controlSet==TEMPO/NUM_KNOBS) {
          byte tempoKnobNum = TEMPO%NUM_KNOBS;
          bitWrite(knobLocked, tempoKnobNum, true);
          initValues[tempoKnobNum] = analogValues[tempoKnobNum];
        }
      }
  */
  /*
    #ifdef POT_VOL
    // Need a value between 0 and 254 for the volume
    byte pot2 = analogRead (POT_VOL) >> 2;
    if (pot2 > 254) pot2 = 254;
    if (pot2 != volume) {
      // Set the volume on both channels.
      //
      // Note that the volume setting is actually an attenuation
      // setting, which means the loudest is 0 (no attenuation)
      // and the quietest is 254 or 0xFE (full attenuation).
      //
      // To preserve the "pot at zero, volume at zero" feeling
      // we reverse the values here.
      //
      VSWriteRegister(SCI_VOL, 254-pot2, 254-pot2);
      volume = pot2;
    }
    #endif // POT_VOL*/


}




/**** Euclidean rythm algos ****/

uint8_t getStepNumber() {
  return stepCounter;
};
uint8_t getNumberOfFills() {
  return numberOfFills;
};

void generateSequence( uint8_t fills, uint8_t steps) {
  numberOfSteps = steps;
  if (fills > steps) fills = steps;
  if (fills <= steps) {
    for (int i = 0; i < 32; i++) euclidianPattern[i] = false;
    if (fills != 0) {
      euclidianPattern[0] = true;
      float coordinate = (float)steps / (float)fills;
      float whereFloat = 0;
      while (whereFloat < steps) {
        uint8_t where = (int)whereFloat;
        if ((whereFloat - where) >= 0.5) where++;
        euclidianPattern[where] = true;
        whereFloat += coordinate;
      }
    }
  }
}

void generateRandomSequence( uint8_t fills, uint8_t steps) {
  //if(numberOfSteps!=steps && numberOfFills!=fills){
  numberOfSteps = steps;
  numberOfFills = fills;
  if (fills > steps) fills = steps;
  if (fills <= steps) {
    for (int i = 0; i < 32; i++) euclidianPattern[i] = false;
    //euclidianPattern[17]=true;
    if (fills != 0) {
      //  euclidianPattern[0]=true;
      //Serial.println();
      for (int i = 0; i < fills; i++) {
        uint8_t where;
        //if(euclidianPattern[where]==false) euclidianPattern[where]=true;//, Serial.print(where);
        //else{
        while (1) {
          where = random(steps);
          if (euclidianPattern[where] == false) {
            euclidianPattern[where] = true; //,Serial.print(where),Serial.print(" ");
            break;
          }

        }
        //}
      }
      //Serial.println();
    }
  }
  //}
}


void rotate(uint8_t _steps) {
  for (int i = 0; i < _steps; i++) {
    bool temp = euclidianPattern[numberOfSteps];
    for (int j = numberOfSteps; j > 0; j--) {
      euclidianPattern[j] = euclidianPattern[j - 1];
    }
    euclidianPattern[0] = temp;
  }
}
bool getStep(uint8_t _step) {
  return euclidianPattern[_step];
}
bool getCurrentStep() {
  return euclidianPattern[stepCounter];
}
void doStep() {
  if (stepCounter < (numberOfSteps - 1)) stepCounter++;
  else stepCounter = 0;
}

void resetSequence() {
  stepCounter = 0;
}

/***********************************************************************************************

   Here is the code to send MIDI data to the VS1053 over the SPI bus.

   Taken from MP3_Shield_RealtimeMIDI.ino by Matthias Neeracher
   which was based on Nathan Seidle's Sparkfun Electronics example code for the Sparkfun
   MP3 Player and Music Instrument shields and and VS1053 breakout board.

 ***********************************************************************************************
*/
void sendMIDI(byte data) {
  SPI.transfer(0);
  SPI.transfer(data);
}

void talkMIDI(byte cmd, byte data1, byte data2) {
  //
  // Wait for chip to be ready (Unlikely to be an issue with real time MIDI)
  //
  while (!digitalRead(VS_DREQ)) {
  }
  digitalWrite(VS_XDCS, LOW);

  sendMIDI(cmd);

  //Some commands only have one data byte. All cmds less than 0xBn have 2 data bytes
  //(sort of: http://253.ccarh.org/handout/midiprotocol/)
  // SysEx messages (0xF0 onwards) are variable length but not supported at present!
  if ( (cmd & 0xF0) <= 0xB0 || (cmd & 0xF0) >= 0xE0) {
    sendMIDI(data1);
    sendMIDI(data2);
  } else {
    sendMIDI(data1);
  }

  digitalWrite(VS_XDCS, HIGH);
}


/***********************************************************************************************

   Code from here on is the magic required to initialise the VS10xx and
   put it into real-time MIDI mode using an SPI-delivered patch.

   Here be dragons...

   Based on VS1003b/VS1033c/VS1053b Real-Time MIDI Input Application
   http://www.vlsi.fi/en/support/software/vs10xxapplications.html

   With some input from MP3_Shield_RealtimeMIDI.ino by Matthias Neeracher
   which was based on Nathan Seidle's Sparkfun Electronics example code for the Sparkfun
   MP3 Player and Music Instrument shields and and VS1053 breakout board.

 ***********************************************************************************************
*/

void initialiseVS10xx () {
  // Set up the pins controller the SPI link to the VS1053
  pinMode(VS_DREQ, INPUT);
  pinMode(VS_XCS, OUTPUT);
  pinMode(VS_XDCS, OUTPUT);
  pinMode(VS_RESET, OUTPUT);

  // Setup SPI
  // The Arduino's Slave Select pin is only required if the
  // Arduino is acting as an SPI slave device.
  // However, the internal circuitry for the ATmeta328 says
  // that if the SS pin is low, the MOSI/MISO lines are disabled.
  // This means that when acting as an SPI master (as in this case)
  // the SS pin must be set to an OUTPUT to prevent the SPI lines
  // being automatically disabled in hardware.
  // We can still use it as an OUTPUT IO pin however as the value
  // (HIGH or LOW) is not significant - it just needs to be an OUTPUT.
  // See: http://www.gammon.com.au/spi
  //
  pinMode(VS_SS, OUTPUT);

  // Now initialise the VS10xx
  digitalWrite(VS_XCS, HIGH);  //Deselect Control
  digitalWrite(VS_XDCS, HIGH); //Deselect Data
  digitalWrite(VS_RESET, LOW); //Put VS1053 into hardware reset

  // And then bring up SPI
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);

  //From page 12 of datasheet, max SCI reads are CLKI/7. Input clock is 12.288MHz.
  //Internal clock multiplier is 1.0x after power up.
  //Therefore, max SPI speed is 1.75MHz. We will use 1MHz to be safe.
  SPI.setClockDivider(SPI_CLOCK_DIV16); //Set SPI bus speed to 1MHz (16MHz / 16 = 1MHz)
  SPI.transfer(0xFF); //Throw a dummy byte at the bus

  delayMicroseconds(1);
  digitalWrite(VS_RESET, HIGH); //Bring up VS1053

  // Dummy read to ensure VS SPI bus in a known state
  VSReadRegister(SCI_MODE);

  // Perform software reset and initialise VS mode
  VSWriteRegister16(SCI_MODE, SM_SDINEW | SM_RESET);
  delay(200);
  VSStatus();
#ifdef TEST

#ifdef SINTEST
  // Output a test sine wave to check everything is working ok
  VSSineTest();
  delay(100);
  VSStatus();
#endif // SINTEST
#endif // TEST

  // Enable real-time MIDI mode
  VSLoadUserCode();
  VSStatus();

  // Set the default volume
  VSWriteRegister(SCI_VOL, 0x00, 0x00);  // 0 = Maximum; 0xFEFE = silence
  VSStatus();
}

// This will read key status and mode registers from the VS10xx device
// and dump them to the serial port.
//
void VSStatus (void) {
#ifdef TEST
  // Print out some of the VS10xx registers
  uint16_t vsreg = VSReadRegister(SCI_MODE); // MODE Mode Register
  sprintf(teststr, "Mode=0x%04x b", vsreg);
  Serial.print(teststr);
  Serial.println(vsreg, BIN);
  vsreg = VSReadRegister(SCI_STATUS);
  sprintf(teststr, "Stat=0x%04x b", vsreg);
  Serial.print(teststr);
  Serial.print(vsreg, BIN);
  switch (vsreg & SS_VER_MASK) {
    case SS_VER_VS1001: Serial.println(" (VS1001)"); break;
    case SS_VER_VS1011: Serial.println(" (VS1011)"); break;
    case SS_VER_VS1002: Serial.println(" (VS1002)"); break;
    case SS_VER_VS1003: Serial.println(" (VS1003)"); break;
    case SS_VER_VS1053: Serial.println(" (VS1053)"); break;
    case SS_VER_VS1033: Serial.println(" (VS1033)"); break;
    case SS_VER_VS1063: Serial.println(" (VS1063)"); break;
    case SS_VER_VS1103: Serial.println(" (VS1103)"); break;
    default: Serial.println(" (Unknown)"); break;
  }
  vsreg = VSReadRegister(SCI_VOL); // VOL Volume
  sprintf(teststr, "Vol =0x%04x\n", vsreg);
  Serial.print(teststr);
  vsreg = VSReadRegister(SCI_AUDATA); // AUDATA Misc Audio data
  sprintf(teststr, "AUDA=0x%04x (%uHz)\n", vsreg, (vsreg & 0xFFFE));
  Serial.print(teststr);
  Serial.println();
#endif
}

// This sends a special sequence of bytes to the device to
// get it to output a test sine wave.
//
// See the datasheets for details.
//
void VSSineTest () {
  VSWriteRegister16(SCI_MODE, SM_SDINEW | SM_RESET | SM_TESTS);
  delay(100);
  while (!digitalRead(VS_DREQ)) ; //Wait for DREQ to go high indicating IC is available
  digitalWrite(VS_XDCS, LOW); //Select control

  //Special 8-byte sequence to trigger a sine test
  SPI.transfer(0x53);
  SPI.transfer(0xef);
  SPI.transfer(0x6e);
  SPI.transfer(0x63);  // FIdx(7:5)=b011; S(4:0)=b00011 ==> F= ~517Hz
  SPI.transfer(0);
  SPI.transfer(0);
  SPI.transfer(0);
  SPI.transfer(0);
  while (!digitalRead(VS_DREQ)) ; //Wait for DREQ to go high indicating command is complete
  digitalWrite(VS_XDCS, HIGH); //Deselect Control

  delay (2000);

  while (!digitalRead(VS_DREQ)) ; //Wait for DREQ to go high indicating IC is available
  digitalWrite(VS_XDCS, LOW); //Select control

  //Special 8-byte sequence to disable the sine test
  SPI.transfer(0x45);
  SPI.transfer(0x78);
  SPI.transfer(0x69);
  SPI.transfer(0x74);
  SPI.transfer(0);
  SPI.transfer(0);
  SPI.transfer(0);
  SPI.transfer(0);
  while (!digitalRead(VS_DREQ)) ; //Wait for DREQ to go high indicating command is complete
  digitalWrite(VS_XDCS, HIGH); //Deselect Control

  delay(100);
  VSWriteRegister16(SCI_MODE, SM_SDINEW | SM_RESET);
  delay(200);
}

// Write to VS10xx register
// SCI: Data transfers are always 16bit. When a new SCI operation comes in
// DREQ goes low. We then have to wait for DREQ to go high again.
// XCS should be low for the full duration of operation.
//
void VSWriteRegister(unsigned char addressbyte, unsigned char highbyte, unsigned char lowbyte) {
  while (!digitalRead(VS_DREQ)) ; //Wait for DREQ to go high indicating IC is available
  digitalWrite(VS_XCS, LOW); //Select control

  //SCI consists of instruction byte, address byte, and 16-bit data word.
  SPI.transfer(0x02); //Write instruction
  SPI.transfer(addressbyte);
  SPI.transfer(highbyte);
  SPI.transfer(lowbyte);
  while (!digitalRead(VS_DREQ)) ; //Wait for DREQ to go high indicating command is complete
  digitalWrite(VS_XCS, HIGH); //Deselect Control
}

// 16-bit interface to the above function.
//
void VSWriteRegister16 (unsigned char addressbyte, uint16_t value) {
  VSWriteRegister (addressbyte, value >> 8, value & 0xFF);
}

// Read a VS10xx register using the SCI (SPI command) bus.
//
uint16_t VSReadRegister(unsigned char addressbyte) {
  while (!digitalRead(VS_DREQ)) ; //Wait for DREQ to go high indicating IC is available
  digitalWrite(VS_XCS, LOW); //Select control

  SPI.transfer(0x03); //Read instruction
  SPI.transfer(addressbyte);
  delayMicroseconds(10);
  uint8_t d1 = SPI.transfer(0x00);
  uint8_t d2 = SPI.transfer(0x00);
  while (!digitalRead(VS_DREQ)) ; //Wait for DREQ to go high indicating command is complete
  digitalWrite(VS_XCS, HIGH); //Deselect control

  return ((d1 << 8) | d2);
}

// Load a user plug-in over SPI.
//
// See the application and plug-in notes on the VLSI website for details.
//
void VSLoadUserCode(void) {
#ifdef TEST
  Serial.print("Loading User Code");
#endif
  for (int i = 0; i < VS10xx_CODE_SIZE; i++) {
    uint8_t addr = pgm_read_byte_near(&vs10xx_atab[i]);
    uint16_t dat = pgm_read_word_near(&vs10xx_dtab[i]);
#ifdef TEST
    if (!(i % 8)) Serial.print(".");
    //    sprintf(teststr, "%4d --> 0x%04X => 0x%02x\n", i, dat, addr);
    //    Serial.print(teststr);
#endif
    VSWriteRegister16 (addr, dat);
  }

  // Set the start address of the application code (see rtmidi.pdf application note)
#ifdef VS1003_MODULE
  VSWriteRegister16(SCI_AIADDR, 0x30);
#endif
#ifdef VS1053_MP3_SHIELD
  VSWriteRegister16(SCI_AIADDR, 0x50);
#endif

#ifdef TEST
  Serial.print("Done\n");
#endif
}

#ifdef TEST
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// A set of test functions that can be used to probe the voices
// used on less capable modules where voices are mapped over to GM voice sets.
//
// Can be ignored for most practical purposes!
//
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void checkVoices () {
  // Voices to check - range 1 to 128
  for (int i = 120; i < 129; i++) {
    Serial.print("Voice: ");
    Serial.println(i);
    testVoice(i);
  }
}

void probeVoice (int voice) {
  int ins[]  = {  1,  9, 17, 25, 30, 33, 41, 49, 57, 65, 73, 81, 89, 113}; // 1 to 128; change to 0 to 127 in msg
  char vce[] = {'P', 'V', 'O', 'G', 'g', 'B', 'v', 'S', 'T', 'x', 'F', 'L', 'd', 's'};
  Serial.print("Probing voice: ");
  Serial.println(voice);
  for (int i = 0; i < (sizeof(ins) / sizeof(ins[0])); i++) {
    Serial.print("Voice: ");
    Serial.print(ins[i]);
    Serial.print(" ");
    Serial.println(vce[i]);
    testVoice(ins[i]);
    testVoice(voice);
  }
  Serial.print("\n");
}

// voice is 1 to 128
void testVoice (int voice) {
  talkMIDI(0xC0, voice - 1, 0);
  talkMIDI (0x90, 60, 127);
  delay(200);
  talkMIDI (0x90, 64, 127);
  delay(200);
  talkMIDI (0x90, 67, 127);
  delay(200);
  talkMIDI (0x80, 60, 0);
  talkMIDI (0x80, 64, 0);
  talkMIDI (0x80, 67, 0);
  delay(200);
}
#endif
