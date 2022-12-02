// Project AlexMod. Custom firmware for Alexmos SimpleBGC expansion board.
// Using SPWM technique for BLDC position controlling.
// Coded by TinLethax 2022/11/17 +7

#include <avr/power.h>

// Motor Control pinout
// ATTiny -> L6234
// PB0 -> IN1
// PB4 -> IN2
// PB2 -> IN3
#define BLDC_U  0
#define BLDC_V  4
#define BLDC_W  2

#include "lut.h"

uint8_t sinU = 0;
uint8_t sinV = 0;
uint8_t sinW = 0;

// I2C stuffs
// I2C Pinout
// PA0 -> SDA
// PA2 -> SCL
#define SDA 0
#define SCL 2

volatile uint8_t i2cfsm = 0;
#define USI_SLAVE_CHECK_ADDRESS                 1
#define USI_SLAVE_CHECK_REPLY_FROM_SEND_DATA    2
#define USI_SLAVE_SEND_DATA                     3
#define USI_SLAVE_REQUEST_REPLY_FROM_SEND_DATA  4
#define USI_SLAVE_REQUEST_DATA                  5
#define USI_SLAVE_GET_DATA_AND_SEND_ACK         6

// I2C slave address (7bit)
#define I2C_ADDR  0x30

// I2C buffer
#define MAX_CMD_BYTE  4 // Command buffer lenght
volatile uint8_t i2cbuf[MAX_CMD_BYTE] = {0};
volatile uint8_t i2ccnt = 0;
uint8_t dataflag = 0;

volatile uint16_t step_cnt = 0;
volatile uint16_t step_max = 0;
#define DEFAULT_STEP_DELAY 80
volatile uint16_t step_delay = DEFAULT_STEP_DELAY;

volatile uint8_t main_fsm = 0;

void initGPIO(){
  // Init Motor Pin
  DDRB |= (1 << BLDC_U) | (1 << BLDC_V) | (1 << BLDC_W);
  PORTB &= ~((1 << BLDC_U) | (1 << BLDC_V) | (1 << BLDC_W));
  // Init LED pin
  DDRA |= (1 << 4);  
}

void initUSI(){
  // Set up the Universal Serial interface as I2C
  PORTA |= (1 << SCL) | (1 << SDA);
  DDRA |= (1 << SCL);
  DDRA &= ~(1 << SDA);
  USIPP = 0x01;// Use PA0 and PA2 as SDA and SCL.
  USICR = 0xA8;
  USISR = 0xF0;  
}

void initTimer1(){
  // Setup Timer 1 PWM
  // Enable Timer clock Sync mode by disable PCKE
  PLLCSR |= (1 << 2);

  OCR1C = 0xFF; // Counter max

  // Set clock prescaler and Invert PWM value
  TCCR1B = 0x83;// Sync clock mode : CK/4
  // Set Fast PWM Mode
  TCCR1D = 0x01;// Enable WGM10 bit (Fast PWM mode)
  TCCR1A = 0x03;// Enable PWM1A PWM1B
  TCCR1C = 0x01;// Enable PWM1D
  TCCR1C |= 0x55;// Enable COM1A0, COM1B0 and COM1D0
}

void setup() {
  // put your setup code here, to run once:
  
  initGPIO();
  initUSI();
  initTimer1();

  // Initial PWM value
  OCR1B = 0;// U phase
  OCR1A = 0;// V phase
  OCR1D = 0;// W phase

  // Initial Sine angle in terms of LUT address.
  // LUT address = 360/256 * Theta ; Theta is angle in degree.
  sinU = 0;
  sinV = 85;
  sinW = 170;

  sei();// Enable Global interrupt
}

ISR(USI_START_vect) {
  i2cfsm = USI_SLAVE_CHECK_ADDRESS;
  DDRA &= ~(1 << SDA);
  while ((PINA & (1 << SCL)) && !(PINA & (1 << SDA)));

  if (PINA & ( 1 << SCL)) {
    USICR = 0xA8;
  } else {
    USICR = 0xF8;
  }

  USISR = 0xF0;
}

ISR(USI_OVF_vect) {
  switch (i2cfsm) {
    case USI_SLAVE_CHECK_ADDRESS :
      if ((USIDR == 0) || ((USIDR >> 1) == I2C_ADDR)) {
        if (USIDR & 0x01) {
          i2cfsm = USI_SLAVE_SEND_DATA;
        } else {
          i2cfsm = USI_SLAVE_REQUEST_DATA;
        }

        USIDR = 0;
        DDRA |= (1 << SDA);
        USISR = 0x7E;
      } else {
        DDRA &= ~(1 << SDA);
        USICR = 0xA8;
        USISR = 0x70;
      }

      step_cnt = 0;
      step_delay = DEFAULT_STEP_DELAY;// default motor step delay
      main_fsm = 0;

      break;

    case USI_SLAVE_CHECK_REPLY_FROM_SEND_DATA :
      if (USIDR) {
        DDRA &= ~(1 << SDA);
        USICR = 0xA8;
        USISR = 0x70;
        return;
      }

    case USI_SLAVE_SEND_DATA :
      // TODO
      if (i2ccnt != (MAX_CMD_BYTE-1)) {
        USIDR = i2cbuf[i2ccnt++];
      } else {
        USIDR = i2cbuf[i2ccnt];
        i2ccnt = 0;
        DDRA &= ~(1 << SDA);
        USICR = 0xA8;
        USISR = 0x70;
        return;
      }

      i2cfsm = USI_SLAVE_REQUEST_REPLY_FROM_SEND_DATA;
      DDRA |= (1 << SDA);
      USISR = 0x70;
      break;

    case USI_SLAVE_REQUEST_REPLY_FROM_SEND_DATA :
      i2cfsm = USI_SLAVE_CHECK_REPLY_FROM_SEND_DATA;
      DDRA &= ~(1 << SDA);
      USIDR = 0;
      USISR = 0x7E;
      break;

    case USI_SLAVE_REQUEST_DATA :
      i2cfsm = USI_SLAVE_GET_DATA_AND_SEND_ACK;
      DDRA &= ~(1 << SDA);
      USISR = 0x70;

      if (i2ccnt != MAX_CMD_BYTE) {
        while ((USISR & 0xAE) == 0);
        if (USISR & (1 << 5)) {
          i2ccnt = 0;
        }
      }

      break;

    case USI_SLAVE_GET_DATA_AND_SEND_ACK :
      i2cfsm = USI_SLAVE_REQUEST_DATA;

      if (i2ccnt != (MAX_CMD_BYTE-1)) {
        i2cbuf[i2ccnt++] = USIDR;

        USIDR = 0;
        DDRA |= (1 << SDA);
        USISR = 0x7E;
      } else {
        i2cbuf[i2ccnt] = USIDR;
        i2ccnt = 0;
        dataflag = 1;
        DDRA &= ~(1 << SDA);
        USISR = 0x7E;
      }

      break;
  }
}

void loop() {

  switch (main_fsm) {
    case 0:// IDLE, wait for command from I2C host
      if (dataflag == 1) {
        // If step is 0. Just dont move the motor.
        if((i2cbuf[1] | i2cbuf[2]) == 0)
          break;

        PORTA |= (1 << 4);// Turn LED status on.
        
        dataflag = 0;// clear data flag.

        // Set maximum step for counting.
        // My motor spin 360 degree with 1792 steps
        // FYI. I use 2204 260kV gimbal motor.
        step_max = i2cbuf[2] << 8;
        step_max |= i2cbuf[1];

        step_delay = i2cbuf[3] * 80;
        if(step_delay < DEFAULT_STEP_DELAY)
          step_delay = DEFAULT_STEP_DELAY;
        
        switch (i2cbuf[0] & 0xC0) {
          case 0x40:// Step backward
            OCR1B = pgm_read_byte(&lut[sinU]);
            OCR1A = pgm_read_byte(&lut[sinV]);
            OCR1D = pgm_read_byte(&lut[sinW]);
            main_fsm = 1;
            break;

          case 0x80:// Step forward
            OCR1B = pgm_read_byte(&lut[sinU]);
            OCR1A = pgm_read_byte(&lut[sinV]);
            OCR1D = pgm_read_byte(&lut[sinW]);
            main_fsm = 2;
            break;
        }
      }

      break;

    case 1:// Step forward
      cli();
      OCR1B = pgm_read_byte(&lut[sinU++]);
      OCR1A = pgm_read_byte(&lut[sinV++]);
      OCR1D = pgm_read_byte(&lut[sinW++]);
      sei();
      step_cnt++;
      
      delayMicroseconds(step_delay);

      if (step_cnt == step_max) {
        main_fsm = 3;
        break;
      }

      break;

    case 2:// Step backward
      cli();
      OCR1B = pgm_read_byte(&lut[sinU--]);
      OCR1A = pgm_read_byte(&lut[sinV--]);
      OCR1D = pgm_read_byte(&lut[sinW--]);
      sei();
      step_cnt++;
      delayMicroseconds(step_delay);

      if (step_cnt == step_max) {
        main_fsm = 3;
        break;
      }

      break;

    case 3:// go back to main_fsm = 0
      PORTA &= ~(1 << 4);
      main_fsm = 0;
      step_cnt = 0x00;

      break;
  }
  
}
