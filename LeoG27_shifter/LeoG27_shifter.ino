#include <HX711.h>
#include <Joystick.h>

//  G27 shifter to USB interface
//  based on a Sparkfn Pro Micro
//  Original by pascalh http://insidesimracing.tv/forums/topic/13189-diy-g25-shifter-interface-with-h-pattern-sequential-and-handbrake-modes/
//  Adapted by jssting for the Sparkfun
//  Updated to include the POV Hat switch

//  2 operating mode
//  - H-pattern shifter
//  - Analog Handbrake 

//  G27 shifter pinout 
// NB!!!!- Ensure you get the VCC and GND pinout correctly assigned to prevent damage to the shifter.
//  DB9     Color   Description         Pro Micro PIN Leonardo PIN
//  1       Yellow  SCK Button Clock    Pin 15        Pin 5
//  2       Grey    MISO Button Data    Pin 14        Pin 2
//  3       Purple  CS Button !CS&!PL   Pin 16        Pin 3
//  4       Orange  Shifter X axis      A0            A0
//  5       White   SPI input (LED Pin) Pin 9            13
//  6       Black   GND                 GND           GND 
//  7       Red     VCC 5V              VCC           VCC
//  8       Green   Shifter Y axis      A1            A1
//  9       Not Connected
//  Other Items
//  Item                Pin
//  HandBrake           A3

// Pin definitions - update to reflect your pinouts 
#define LED_PIN            5
#define DATA_IN_PIN        16
#define MODE_PIN           14
#define CLOCK_PIN          15
#define X_AXIS_PIN         A0
#define Y_AXIS_PIN         A1

// HX711 circuit wiring
#define LOADCELL_DOUT_PIN A3 //Handbrake Pin
#define LOADCELL_SCK_PIN  10

// H-shifter mode analog axis thresholds 
// adjust these to see the points of transition 
// between the neutral point and the gear engagig
#define HS_XAXIS_12        325 //Gears 1,2
#define HS_XAXIS_56        675 //Gears 5,6, R
#define HS_YAXIS_135       735 //Gears 1,3,5
#define HS_YAXIS_246       325 //Gears 2,4,6, R

// Digital and Analog inputs definitions 
// DI_ = Digital Input
// AI_ = Analog Input
#define DI_REVERSE         1

#define DI_RED_CENTERRIGHT 4
#define DI_RED_CENTERLEFT  5
#define DI_RED_RIGHT       6
#define DI_RED_LEFT        7
#define DI_BLACK_TOP       8
#define DI_BLACK_RIGHT     9
#define DI_BLACK_LEFT      10
#define DI_BLACK_BOTTOM    11
#define DI_DPAD_RIGHT      12
#define DI_DPAD_LEFT       13
#define DI_DPAD_BOTTOM     14
#define DI_DPAD_TOP        15

bool enableHandbrake = true;// set to true if you have the handbrake connected

Joystick_ Joystick(0x08,JOYSTICK_TYPE_JOYSTICK , 16, 1,
 false, false, enableHandbrake, false, false, false,
 false, false, false, false, false);

HX711 scale;

bool enableDebug = false;

void setup()
{
  // G27 shifter analog inputs configuration
  pinMode(X_AXIS_PIN, INPUT_PULLUP);   // X axis
  pinMode(Y_AXIS_PIN, INPUT_PULLUP);   // Y axis
 // pinMode(HANDBRAKE_PIN, INPUT_PULLUP);
  
  // G27 shift register interface configuration
  pinMode(DATA_IN_PIN, INPUT);         // Data in
  pinMode(MODE_PIN, OUTPUT);           // Parallel/serial mode
  pinMode(CLOCK_PIN, OUTPUT);          // Clock

  // LED output mode configuration
  pinMode(LED_PIN, OUTPUT);            // LED

  // Virtual joystick configuration
  Joystick.begin(true);        // Joystick output is synchronized

  // Virtual serial interface configuration
  if (enableDebug) Serial.begin(38400); // use for debugging

  // Virtual joystick initialization
  Joystick.setXAxis(512);  //512 is center
  Joystick.setYAxis(512);  //512 is center
 
  // Digital outputs initialization
  digitalRead(DATA_IN_PIN);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(MODE_PIN, HIGH);
  digitalWrite(CLOCK_PIN, HIGH);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_gain();
  scale.set_scale(-10000);
  scale.tare();               
}

void loop()
{
  // Reading of button states from G27 shift register
  int b[16];

  digitalWrite(MODE_PIN, LOW);         // Switch to parallel mode: digital inputs are read into shift register
  delayMicroseconds(5);               // Wait for signal to settle
  digitalWrite(MODE_PIN, HIGH);        // Switch to serial mode: one data bit is output on each clock falling edge

   for (int i = 0; i < 16; i++) {         // Iteration over both 8 bit registers
    digitalWrite(CLOCK_PIN, LOW);      // Generate clock falling edge
    delayMicroseconds(5);             // Wait for signal to settle

    b[i] = digitalRead(DATA_IN_PIN);   // Read data bit and store it into bit array

    digitalWrite(CLOCK_PIN, HIGH);     // Generate clock rising edge
    delayMicroseconds(5);             // Wait for signal to settle
    if (enableDebug){ if (b[i]>0) Serial.println("Button " + String(i) + ": "  + String(b[i]));  }
  }
 
  //Hand Brake Section
  if (enableHandbrake){  
    int hbState = 0;
    hbState =   abs(scale.get_units(1));
    // Reading of handbrake position with some deadzone added
    if (hbState > 20 ) Joystick.setZAxis(map(hbState,0,511,0,255));
    else Joystick.setZAxis(0);
  }

  //Shifter Section
  // Reading of shifter position
  int x = analogRead(X_AXIS_PIN);      // X axis
  int y = analogRead(Y_AXIS_PIN);      // Y axis

  // Current gear calculation
  int gear = 0;                        // Default value is neutral
  
  if (x < HS_XAXIS_12) {               // Shifter on the left?
    if (y > HS_YAXIS_135)  gear = 1;  // 1st gear
    if (y < HS_YAXIS_246)  gear = 2;  // 2nd gear 
  }
  else if (x > HS_XAXIS_56) {          // Shifter on the right?
    if (y > HS_YAXIS_135) gear = 5;  // 5th gear
    if (y < HS_YAXIS_246) gear = 6;  // 6th gear    
  }
  else {                              // Shifter is in the middle
    if (y > HS_YAXIS_135) gear = 3;  // 3rd gear
    if (y < HS_YAXIS_246) gear = 4;  // 4th gear
  }
  
  if (gear != 6) b[DI_REVERSE] = 0;  // Reverse gear is allowed only on 6th gear position
  if (b[DI_REVERSE] == 1) gear = 7;  // 6th gear is deactivated if reverse gear is engaged

  // Depress virtual button for current gear
  if (gear > 0) {
     clearGears(Joystick,gear);
     Joystick.setButton(gear - 1, HIGH);
  }
  else clearGears(Joystick,0); // Release virtual buttons for all gears
  
  if (enableDebug) Serial.println("Gear: " + String(gear) + ", Y: " + String(y) + ", X: " + String(x));

  //Hat Switch Section
  for (int i = 12; i < 16; i++)
  {
    //release hat switch
    if (!b[DI_DPAD_TOP] && !b[DI_DPAD_RIGHT] && !b[DI_DPAD_BOTTOM] && !b[DI_DPAD_LEFT]) Joystick.setHatSwitch(0, -1);

    //primary button only directions
    else if (b[DI_DPAD_TOP] && !b[DI_DPAD_RIGHT] && !b[DI_DPAD_LEFT]) Joystick.setHatSwitch(0, 0);
    else if (b[DI_DPAD_RIGHT] && !b[DI_DPAD_TOP] && !b[DI_DPAD_BOTTOM]) Joystick.setHatSwitch(0, 90);
    else if (b[DI_DPAD_BOTTOM] && !b[DI_DPAD_RIGHT] && !b[DI_DPAD_LEFT]) Joystick.setHatSwitch(0, 180);
    else if (b[DI_DPAD_LEFT] && !b[DI_DPAD_BOTTOM] && !b[DI_DPAD_TOP]) Joystick.setHatSwitch(0, 270);

    //combined button directions
    else if (b[DI_DPAD_TOP] && b[DI_DPAD_RIGHT]) Joystick.setHatSwitch(0, 45);
    else if (b[DI_DPAD_RIGHT] && b[DI_DPAD_BOTTOM]) Joystick.setHatSwitch(0, 135);
    else if (b[DI_DPAD_BOTTOM] && b[DI_DPAD_LEFT]) Joystick.setHatSwitch(0, 225);
    else if (b[DI_DPAD_TOP] && b[DI_DPAD_LEFT]) Joystick.setHatSwitch(0, 315);
  }

  // Set state of virtual buttons for all the physical buttons (Excluding Gears and the hat switch)
  for (int i = 4; i < 12; i++) Joystick.setButton(3 + i, b[i]);
} 

void clearGears(Joystick_ Joystick, int gearToIgnore){
  for(int i=1 ;i<8;i++)
  {
    if (gearToIgnore<1) Joystick.releaseButton(i-1);   
    else if (i!=gearToIgnore) Joystick.releaseButton(i-1);   
  }   
}
