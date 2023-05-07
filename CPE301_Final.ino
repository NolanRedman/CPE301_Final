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

#define RDA 0x80
#define TBE 0x20  

// Memory address for the ADC
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

// Memory address for the LEDs
volatile unsigned char* port_a = (unsigned char*) 0x22;
volatile unsigned char* ddr_a = (unsigned char*) 0x21;
volatile unsigned char* pin_a = (unsigned char*) 0x20;

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
int current_state = 0;

void setup() {
  Serial.begin(9600);
  current_state = 1;

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
}

void loop() {
  // DateTime now = rtc.now();

  // Serial.print(now.hour(), DEC);
  // Serial.print(':');
  // Serial.print(now.minute(), DEC);
  // Serial.print(':');
  // Serial.println(now.second(), DEC);
  
  // unsigned int adc_reading = adc_read(2);
  // Serial.print("Water sensor: ");
  // Serial.println(adc_reading);

  // int chk = DHT.read11(12);
  // Serial.print("Temp: ");
  // Serial.println(DHT.temperature);
  // Serial.print("Humidity: ");
  // Serial.println(DHT.humidity);

  // lcd.clear();
  // lcd.print("Temp: ");
  // lcd.print(DHT.temperature);
  // lcd.setCursor(0, 1);
  // lcd.print("Humidity: ");
  // lcd.print(DHT.humidity);

  // delay(1000);

  myStepper.setSpeed(10);
	myStepper.step(stepsPerRevolution);
  
  
  if (current_state == 0) {
    // Disabled

    // Fan off
    // Yellow LED on
    *port_a &= 0x00;
    *port_a |= 0x01;

    Serial.println("hi");

    // If button pressed, transition to Idle
  }
  else if (current_state == 1) {
    // Idle

    // Fan off
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

    // if stop is pressed, transistion to disabled

    // If temp > threshold transition to running

    if (DHT.temperature > 26) {
      current_state = 2;
      //start fan motor 

    }
  }
  else if (current_state == 2) {
    // Running

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

      //stop fan motor

    }
  }
  else if (current_state == 3) {
    // Error

    // Red LED on

    // display message "Water level is too low"
    lcd.print("Water level is too low");
  }






}
