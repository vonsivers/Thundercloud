/* 
Lighting Cloud Mood Lamp By James Bruce
View the full tutorial and build guide at http://www.makeuseof.com/

Sound sampling code originally by Adafruit Industries.  Distributed under the BSD license.
This paragraph must be included in any redistribution.
*/

#include <Wire.h>
#include "FastLED.h"

// How many leds in your strip?
#define NUM_LEDS 11
#define DATA_PIN 6

// switch serial output on/off for debugging
// switch off for normal operation
bool verbose = true;

// Mode enumeration - if you want to add additional party or colour modes, add them here; you'll need to map some IR codes to them later; 
// and add the modes into the main switch loop
enum Mode { CLOUD,THUNDER,ACID,OFF,SUNNY,RAINY,FADE,THERMO,WARM,SUNRISE,SUNDOWN};
Mode mode = OFF;  
Mode lastMode = OFF;

// Mic settings, shouldn't need to adjust these. 
#define MIC_PIN   0  // Microphone is attached to this analog pin
int DC_OFFSET = 0;  // DC offset in mic signal 
int NOISE = 5; // Threshold for triggering on mic signal
#define NOISE_UP  20  // Upper limit for triggering on mic signal
#define SAMPLES   5  // Length of buffer for dynamic level adjustment
#define TMP35_PIN 1 // TMP365 is attached to this pin
byte
  volCount  = 0;      // Frame counter for storing past volume data
int
  vol[SAMPLES];       // Collection of prior volume samples
unsigned long sampletime[SAMPLES]; // sample time (for debugging)
unsigned long clocktime; // sample time (for debugging)
int      n, total = 0; // current/total sample reading
float average = 0;

int Tavg;     // temperature values for TMP35
  
// used to make basic mood lamp colour fading feature
int fade_h;
int fade_direction = 1;

// used for sunrise/sundown
double H_sunrise = 160.; 
int B_sunrise = 0;
double H_sundown = 10.; 
int B_sundown = 255;
double H_step = 105./255.; 

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup() { 
  // this line sets the LED strip type - refer fastLED documeantion for more details https://github.com/FastLED/FastLED
  FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
  // starts the audio samples array at volume 15. 
  memset(vol, 15, sizeof(vol));
  Serial.begin(115200);
  Wire.begin(9);                // Start I2C Bus as a Slave (Device Number 9)
  Wire.onReceive(receiveEvent); // register event
}



void receiveEvent(int bytes) {
  
  // Here, we set the mode based on the IR signal received. Check the debug log when you press a button on your remote, and 
  // add the hex code here (you need 0x prior to each command to indicate it's a hex value)
  while(Wire.available())
   { 
      unsigned int received = Wire.read();
      if(verbose) {
        Serial.print("Receiving IR hex: ");
        Serial.println(received,HEX);
      }
      lastMode = mode;
      switch(received){
        case 0x1F:
          resetBH(); mode = OFF; break;
        case 0x2F:
          resetBH(); mode = THUNDER; break;
        case 0x5F:
          resetBH(); record_offset(); mode = CLOUD; break;
        case 0xDF:
          resetBH(); mode = ACID; break;
        case 0x9F:
          resetBH(); mode = FADE; break;
        case 0x6F:
          resetBH(); mode = SUNNY; break;
        case 0xEF:
          resetBH(); mode = RAINY; break;
        case 0xAF:
          resetBH(); mode = THERMO; break;
        case 0x4F:
          resetBH(); mode = WARM; break;
        case 0xCF:
          resetBH(); mode = SUNRISE; break;
        case 0x8F:
          resetBH(); mode = SUNDOWN; break;
        case 0xF7:
          VolDown(); break;
        case 0xB7:
          VolUp(); break;
        
      }
   }

}
 
void loop() { 
  
  // Maps mode names to code functions. 
  switch(mode){
    case CLOUD: detect_thunder();break;
    case ACID: acid_cloud();break;
    case OFF: switchoff();break;
    case THUNDER: constant_lightning();break;
    case SUNNY: sunny();break;
    case RAINY: rainy();break;
    case FADE: colour_fade();break;
    case THERMO: thermometer();break;
    case WARM: setwarm();break;
    case SUNRISE: sunrise(); break;
    case SUNDOWN: sundown(); break;
    default: resetBH();detect_thunder();break; 
  }
  
}

void VolUp() {
  if(NOISE-1>=0) {
    NOISE--;
     if(verbose) {
      Serial.print("Mic Threshold: ");
      Serial.println(NOISE);
    }
  }
}

void VolDown() {
  if(NOISE+1<=NOISE_UP) {
    NOISE++;
    if(verbose) {
      Serial.print("Mic Threshold: ");
      Serial.println(NOISE);
    }
  }
}

void sunny() {
  if(lastMode != mode){
    if(verbose) {
      Serial.println("### Sunny Mode ###");
    }
    colour_shade(0,16,255);
    lastMode = mode;
  }  
}

void rainy() {
  if(lastMode != mode){
    if(verbose) {
      Serial.println("### Rainy Mode ###");
    }
    colour_shade(144,208,255);
    lastMode = mode;
  }
}

void sunrise() {
    lastMode = mode;
    if(verbose) {
      Serial.println("### Sunrise Mode ###");
    }
    if(B_sunrise<=255) {
      for (int i=0;i<NUM_LEDS;i++){
        leds[i] = CHSV( (int)(H_sunrise), 255, B_sunrise);
      }
      if((int)(H_sunrise+H_step)>255) H_sunrise=0.;
      H_sunrise+=H_step;
      B_sunrise++;
      FastLED.show();
      delay(500);
    }
}

void sundown() {
    lastMode = mode;
    if(verbose) {
      Serial.println("### Sundown Mode ###");
    }
    if(B_sundown>0) {
      for (int i=0;i<NUM_LEDS;i++){
        leds[i] = CHSV( (int)(H_sundown), 255, B_sundown);
      }
      if((int)(H_sundown-H_step)<0) H_sundown = 255.;
      H_sundown-=H_step;
      B_sundown--;
      FastLED.show();
      delay(500);
    }
  else {
    mode = OFF;
  }
}

void setwarm() {
  if(lastMode != mode){
     if(verbose) {
      Serial.println("### Warm White Mode ###");
    }
    for (int i=0;i<NUM_LEDS;i++){
      leds[i] = HighPressureSodium;
    }
    FastLED.show(); 
    lastMode = mode;
  }
}

void record_offset(){
   if(verbose) {
      Serial.println("### Recording Mic Offset ###");
    }
  n = 0;
  for(int i=0; i<100; ++i) {
    n += analogRead(MIC_PIN);
  }
  DC_OFFSET = n/100;
  if(verbose) {
    Serial.print("Offset: ");
    Serial.println(DC_OFFSET);
  }
}

void detect_thunder() {

  if(lastMode != mode){
    if(verbose) {
      Serial.println("### Cloud Mode ###");
    }
    colour_shade(150,208,100);
    lastMode = mode;
  }
  n   = analogRead(MIC_PIN);                        // Raw reading from mic 
  clocktime = micros();
  n   = (n - DC_OFFSET < 0) ? 0 : n - DC_OFFSET; // Center on zero
  vol[volCount] = n;                      // Save sample for dynamic leveling
  sampletime[volCount] = clocktime;
  if(++volCount >= SAMPLES) volCount = 0; // Advance/rollover sample counter
 
  total = 0;
  for(int i=0; i<SAMPLES; i++) {
    total += vol[i];
  }
  
  // If you're having trouble getting the cloud to trigger, uncomment this block to output a ton of debug on current averages. 
  // Note that this WILL slow down the program and make it less sensitive due to lower sample rate.

/*
  if(volCount==0) {
  for(int t=0; t<SAMPLES; t++) {
    //initial data is zero. to avoid initial burst, we ignore zero and just add the current l
    Serial.print("sample time (mus): ");
    Serial.print(sampletime[t]-sampletime[0]);
    Serial.print(", reading: ");
    Serial.println(vol[t]);
  }
  
  Serial.print("total ");
  Serial.println(total);
  }
 */

  
  
  //average = (total/SAMPLES);
  if(total>NOISE){
    if(verbose) {
      Serial.println("TRIGGERED");
    }
    //reset();
     
   
    //I've programmed 3 types of lightning. Each cycle, we pick a random one. 
    switch(random(1,3)){
       //switch(3){
  
      case 1:
        thunderburst();
        //delay(random(10,500));
        break;
       
      case 2:
        rolling();
        //delay(random(10,500));
        break;
        
      case 3:
        crack();
        //delay(random(50,250));
        break;
        
      
    }
  }
}

 
void colour_fade(){
  if(lastMode != mode){
    lastMode = mode;
    if(verbose) {
      Serial.println("### Fade Mode ###");
    }
  }
  //mood mood lamp that cycles through colours
  for (int i=0;i<NUM_LEDS;i++){
    leds[i] = CHSV( fade_h, 255, 255);
  }
  if(fade_h >254){
    fade_direction = -1; //reverse once we get to 254
  }
  else if(fade_h < 0){
    fade_direction = 1;
  }
    
  fade_h += fade_direction;
  FastLED.show();
  delay(100);
}

void acid_cloud(){
  if(lastMode != mode){
    lastMode = mode;
    if(verbose) {
      Serial.println("### Acid Mode ###");
    }
  }
    // a modification of the rolling lightning which adds random colour. trippy. 
    //iterate through every LED
    for(int i=0;i<NUM_LEDS;i++){
      if(random(0,100)>90){
        leds[i] = CHSV( random(0,255), 255, 255); 

      }
      else{
        leds[i] = CHSV(0,0,0);
      }
    }
    FastLED.show();
    delay(random(5,100));
    reset();
  //}
}

// basically just a debug mode to show off the lightning in all its glory, no sound reactivity. 
void constant_lightning(){
 
   //avoid flickr which occurs when FastLED.show() is called - only call if the colour changes
  if(lastMode != mode){
    if(verbose) {
      Serial.println("### Constant Lightning Mode ###");
    }
    colour_shade(150,208,100);
    lastMode = mode;
  }
    
  switch(random(1,3)){
      case 1:
        thunderburst();
        //delay(random(10,500));
        break;
       
      case 2:
        rolling();
        //delay(random(10,500));
        break;
        
      case 3:
        crack();
        //delay(random(50,250));
        break;  
  } 
  delay(random(100,1000)); 
}

// read temperature and convert to LED colour
void thermometer() {
if(lastMode != mode){
      if(verbose) {
        Serial.println("### Thermometer Mode ###");
      }
      lastMode = mode;
    }
    // average over 10 values
    Tavg = 0;
    for(int i=0;i<10;++i) {
      Tavg += map(analogRead(TMP35_PIN), 15, 65, 128, 255);
      delay(100);
    }
    Tavg = Tavg/10;
     if(verbose) {
        Serial.print("TMP35 voltage read ");
        Serial.println(Tavg);
      }
    for (int i=0;i<NUM_LEDS;i++){
      leds[i] = CHSV( Tavg, 255, 255);
    }
    FastLED.show(); 
  
}

void switchoff() {
    if(lastMode != mode){
      if(verbose) {
        Serial.println("### Switching LEDs Off ###");
      }
      reset();
      lastMode = mode;
    }
}

// utility function to turn all the lights off.  
void reset(){
  for (int i=0;i<NUM_LEDS;i++){
    leds[i] = CHSV( 0, 0, 0);
  }
  FastLED.show();
}

void rolling(){
  // a simple method where we go through every LED with 1/10 chance
  // of being turned on, up to 10 times, with a random delay wbetween each time
  if(verbose) {
    Serial.println("Rolling");
  }
  for(int r=0;r<random(2,10);r++){
    //iterate through every LED
    for(int i=0;i<NUM_LEDS;i++){
      if(random(0,100)>90){
        leds[i] = CHSV( 0, 0, 255); 

      }
      else{
        //dont need reset as we're blacking out other LEDs her 
        //leds[i] = CHSV(0,0,0);
      }
    }
    FastLED.show();
    delay(random(5,100));
    //reset();
    colour_shade(150,208,100);
  }
}

void crack(){
   if(verbose) {
      Serial.println("Crack");
   }
   //turn everything white briefly
   for(int i=0;i<NUM_LEDS;i++) {
      leds[i] = CHSV( 0, 0, 255);  
   }
   FastLED.show();
   delay(random(10,100));
   //reset();
   //single_colour(180);
   colour_shade(150,208,100);
}

void thunderburst(){
  if(verbose) {
    Serial.println("Thunderburst");
  }
  
  // this thunder works by lighting two random lengths
  // of the strand from 10-20 pixels. 
  int rs1 = random(0,NUM_LEDS/2);
  int rl1 = random(2,5);
  int rs2 = random(rs1+rl1,NUM_LEDS);
  int rl2 = random(2,5);
  
  //repeat this chosen strands a few times, adds a bit of realism
  for(int r = 0;r<random(3,6);r++){
    
    for(int i=0;i< rl1; i++){
      leds[i+rs1] = CHSV( 0, 0, 255);
    }
    
    if(rs2+rl2 < NUM_LEDS){
      for(int i=0;i< rl2; i++){
        leds[i+rs2] = CHSV( 0, 0, 255);
      }
    }
    
    FastLED.show();
    //stay illuminated for a set time
    delay(random(10,50));
    
    //reset();
    //single_colour(180);
    colour_shade(150,208,100);
  }
  
}


// colour shade see https://raw.githubusercontent.com/FastLED/FastLED/gh-pages/images/HSV-rainbow-with-desc.jpg for H values
void colour_shade(double H1, double H2, int brightness){
     if(verbose) {
        Serial.println("setting colour shade");
     }
    double stepsize = (double) (H2-H1)/NUM_LEDS; 
    double H = H1;
    int colour;
    for (int i=0;i<NUM_LEDS;i++){
      colour = (int) H;
      /*
      if(verbose) {
        Serial.print("setting LED ");
        Serial.println(i);
        Serial.print("colour to ");
        Serial.println(colour);
      }
      */
      leds[i] = CHSV( colour, 255, brightness);
      H += stepsize;
    }
    
  FastLED.show(); 
  //delay(50);
}

void resetBH() {
  H_sunrise = 160; 
  B_sunrise = 0; 
  H_sundown = 10; 
  B_sundown = 255;
}

/*
// Makes all the LEDs a single colour, see https://raw.githubusercontent.com/FastLED/FastLED/gh-pages/images/HSV-rainbow-with-desc.jpg for H values
void single_colour(int H){

  //avoid flickr which occurs when FastLED.show() is called - only call if the colour changes
  if(lastMode != mode){
    for (int i=0;i<NUM_LEDS;i++){
      leds[i] = CHSV( H, 255, 255);
    }
    FastLED.show(); 
    lastMode = mode;
  }
  delay(50);
}
*/



