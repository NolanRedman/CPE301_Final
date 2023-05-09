#include <RTClib.h>
#include <dht.h>
#include <LiquidCrystal.h>
#include <Stepper.h>

// Sensors
RTC_DS1307 rtc;
dht DHT;
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

const int stepsPerRevolution = 2038;
Stepper myStepper = Stepper(stepsPerRevolution, 11, 10, 9, 8);

// Memory address for the ADC
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

// Memory address for UART
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;
#define RDA 0x80
#define TBE 0x20 


// Memory address for the LEDs
volatile unsigned char* port_a = (unsigned char*) 0x22;
volatile unsigned char* ddr_a = (unsigned char*) 0x21;
volatile unsigned char* pin_a = (unsigned char*) 0x20;

// Memory address for the fan motor
volatile unsigned char* port_b = (unsigned char*) 0x25;
volatile unsigned char* ddr_b = (unsigned char*) 0x24;
volatile unsigned char* pin_b = (unsigned char*) 0x23;

// Memory address for the buttons
volatile unsigned char* port_g = (unsigned char*) 0x34;
volatile unsigned char* ddr_g = (unsigned char*) 0x33;
volatile unsigned char* pin_g = (unsigned char*) 0x32;

volatile unsigned char* port_d = (unsigned char*) 0x2B;
volatile unsigned char* ddr_d = (unsigned char*) 0x2A;
volatile unsigned char* pin_d = (unsigned char*) 0x29;

volatile int vent_position;

void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}

unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if (adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}

// 0 - disabled, 1 - idle, 2 - running, 3 - error
volatile int current_state = 0;
// Whether or not a transition message needs to be printed
volatile bool transition = 0;

void setup() {
  U0Init(9600);

  current_state = 0;

  // setup the ADC
  adc_init();

  // SETUP RTC MODULE
  rtc.begin();

  // setup the LCD
  lcd.begin(16, 2);

  // Set the RTC to the date & time on PC this sketch was compiled
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Setup the LEDs
  // Set 22-28 to output
  *ddr_a |= 0xFF;

  // Setup the fan port
  *ddr_b |= 0x80;

  // Setup the buttons
  // Set PG0 and PG1 to input
  *ddr_g &= 0xFC;
  // Enable pullup resistor
  *port_g |= 0x03;

  *ddr_d &= 0xF7;
  *port_d |= 0x08;
  // Attach interrupt function to the on/off button
  attachInterrupt(digitalPinToInterrupt(18), ISROnButton, FALLING);
}

volatile bool vent_open = false;

void loop() {

  // If a transition occured, print a message
  if (transition) {
    transition = false;
    U0printTime();
    U0putString(" Transitioned to");
    if (current_state == 0) {
      U0putString(" disabled.");
      U0putChar(10); // newline
    }
    else if (current_state == 1) {
      U0putString(" idle.");
      U0putChar(10);
    }
    else if (current_state == 2) {
      U0putString(" running.");
      U0putChar(10);
    }
    else if (current_state == 2) {
      U0putString(" error.");
      U0putChar(10);
    }
  }

  // Detect when the vent button is pressed, then spin the vent if it is
  if (current_state != 3) {
    if (!(*pin_g & 0x01)) {
      if (vent_open) {
        myStepper.setSpeed(10);
        myStepper.step(-1000);
        vent_open = false;
      }
      else {
        myStepper.setSpeed(10);
        myStepper.step(1000);
        vent_open = true;
      }
    }
  }
  
  if (current_state == 0) {
    // Disabled

    // Fan off
    *port_b &= 0x7F;

    // Yellow LED on
    *port_a &= 0x00;
    *port_a |= 0x01;

    // Clear the lcd screen
    lcd.clear(); 
  }
  else if (current_state == 1) {
    // Idle

    // Fan off
    *port_b &= 0x7F;

    // Green LED on
    *port_a &= 0x00;
    *port_a |= 0x04;

    // temp and humidity should be dislayed on LCD
    int chk = DHT.read11(12);
    lcd.clear();
    lcd.print("Temp: ");
    lcd.print(DHT.temperature);
    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(DHT.humidity);

    // If temp > threshold transition to running
    if (DHT.temperature > 26) {
      current_state = 2;
      transition = true;      
    }

    // If water level < threshold transition to error
    
  }
  else if (current_state == 2) {
    // Running

    // Turn on fan
    *port_b |= 0x80;

    // Blue LED on
    *port_a &= 0x00;
    *port_a |= 0x40;
   
    // temp and humidity should be dislayed on LCD
    int chk = DHT.read11(12);
    lcd.print("Temp: ");
    lcd.print(DHT.temperature);
    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(DHT.humidity);

    // If temp <= threshold transition to idle

    if (DHT.temperature <= 26){
      current_state = 1;
      transition = true;
    }
  }
  else if (current_state == 3) {
    // Error

    // Fan off
    *port_b &= 0x7F;

    // Red LED on

    // display message "Water level is too low"
    lcd.print("Water level is too low");
  }
}

void ISROnButton(void) {
  if (current_state == 3) {
    current_state = 1;
  }
  else if (current_state == 0) {
    current_state = 1;
  }
  else {
    current_state = 0;
  }
  transition = true;
}

// UART Functions
void U0Init(unsigned long U0baud) {
  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1);
  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0 = tbaud;
}

unsigned char U0kbhit() {
  return *myUCSR0A & RDA;
}

unsigned char U0getChar() {
  return *myUDR0;
}

void U0putChar(unsigned char U0pdata) {
  while (!(*myUCSR0A & TBE));
  *myUDR0 = U0pdata;
}

// Custom function to send a string of characters to UART and send newline
void U0putString(char * data) {
  for (int i = 0; i < data[i] != 0; i++) {
    U0putChar(data[i]);
  }
}

// Custom function to send the current time to UART
void U0printTime() {
  DateTime now = rtc.now();
  char hour[256];
  char min[256];
  char sec[256];

  snprintf(hour, sizeof(hour), "%d", now.hour());
  snprintf(min, sizeof(min), "%d", now.minute());
  snprintf(sec, sizeof(sec), "%d", now.second());

  U0putString(hour);
  U0putChar(':');
  U0putString(min);
  U0putChar(':');
  U0putString(sec);
}
