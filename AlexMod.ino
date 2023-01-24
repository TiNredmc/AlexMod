// Project AlexMod. Custom firmware for Alexmos SimpleBGC expansion board.
// Using THSPWM technique for BLDC position controlling.
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

// I2C packet
// Position Mode
// [Direction][Step LSB][Step MSB][Speed LSB][Speed MSB]
// Speed mode
// [Direction][0xFF][0xFF][Speed LSB][Speed MSB]

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
#define MAX_CMD_BYTE  5 // Command buffer lenght
volatile uint8_t i2cbuf[MAX_CMD_BYTE] = {0};
volatile uint8_t i2ccnt = 0;
uint8_t dataflag = 0;

volatile uint16_t step_cnt = 0;
volatile uint16_t step_max = 0;
volatile uint8_t speed = 0;
uint8_t speed_ramp = 0;
uint8_t ramp_fsm = 0;
uint16_t ramp_cnt = 0;

volatile uint8_t main_fsm = 0;

void initGPIO() {
  // Init Motor Pin
  DDRB |= (1 << BLDC_U) | (1 << BLDC_V) | (1 << BLDC_W);
  PORTB &= ~((1 << BLDC_U) | (1 << BLDC_V) | (1 << BLDC_W));
  // Init LED pin
  DDRA |= (1 << 4);
}

void initUSI() {
  // Set up the Universal Serial interface as I2C
  PORTA |= (1 << SCL) | (1 << SDA);
  DDRA |= (1 << SCL);
  DDRA &= ~(1 << SDA);
  USIPP = 0x01;// Use PA0 and PA2 as SDA and SCL.
  USICR = 0xA8;
  USISR = 0xF0;
}

void initTimer0(uint8_t HZ) {
  TCCR0A = 0x01;// Enable CTC mode
//  if(HZ > 1953)// Max sine frequency is 1.953kHz
//    return;
    
  //HZ <<= 3;// Multiply by 8 with bit shifting
  
  // CLK/1024 clock divider
  TCCR0B = 0x05;
  OCR0A = (uint8_t)(HZ - 1); 
  
  TIMSK |= (1 << 4);
}

void deinitTimer0() {
  TIFR = (1 << 4);
  TIMSK = 0x00;
  TCCR0A = 0x00;
  TCCR0B = 0x00;
  OCR0A = 0;
}

void initTimer1() {
  // Setup Timer 1 PWM
  // Enable Timer clock Sync mode by disable PCKE
  PLLCSR |= (1 << 2);

  OCR1C = 0xFF; // Counter max

  // Set clock prescaler and Invert PWM value
  TCCR1B = 0x82;// Sync clock mode : CK/2
  // Set Fast PWM Mode
  TCCR1D = 0x01;// Enable WGM10 bit (Dual slope PWM mode)
  TCCR1A = 0x03;// Enable PWM1A PWM1B
  TCCR1C = 0x01;// Enable PWM1D
  TCCR1C |= 0x55;// Enable COM1A0, COM1B0 and COM1D0
}

void setup() {
  // put your setup code here, to run once:
  clock_prescale_set(clock_div_1);
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

      TIFR = (1 << 4);
      main_fsm = 8;

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
      if (i2ccnt != (MAX_CMD_BYTE - 1)) {
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

      if (i2ccnt != (MAX_CMD_BYTE - 1)) {
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

ISR(TIMER0_COMPA_vect) {
  //PORTA |= (1 << 4);// Turn LED status on.
  switch (main_fsm) {
    case 1:// Step forward
      OCR1B = pgm_read_byte(&lut[sinU++]);
      OCR1A = pgm_read_byte(&lut[sinV++]);
      OCR1D = pgm_read_byte(&lut[sinW++]);
      step_cnt++;

      if (step_cnt == step_max) {
        main_fsm = 8;
        break;
      }

      break;

    case 2:// Step backward
      OCR1B = pgm_read_byte(&lut[sinU--]);
      OCR1A = pgm_read_byte(&lut[sinV--]);
      OCR1D = pgm_read_byte(&lut[sinW--]);
      step_cnt++;

      if (step_cnt == step_max) {
        main_fsm = 8;
        break;
      }

      break;

    case 3:// Speed forward
      OCR1B = pgm_read_byte(&lut[sinU]);
      OCR1A = pgm_read_byte(&lut[sinV]);
      OCR1D = pgm_read_byte(&lut[sinW]);
      
      sinU += 32;
      sinV += 32;
      sinW += 32;
      
      break;

    case 4:// Speed backward
      OCR1B = pgm_read_byte(&lut[sinU]);
      OCR1A = pgm_read_byte(&lut[sinV]);
      OCR1D = pgm_read_byte(&lut[sinW]);
      
      sinU -= 32;
      sinV -= 32;
      sinW -= 32;
      
      break;
  }

}

void loop() {

  switch (main_fsm) {
    case 0:// IDLE, wait for command from I2C host
      if (dataflag == 1) {
        // If step is 0. Just dont move the motor.
        if ((i2cbuf[1] | i2cbuf[2]) == 0)
          break;

        dataflag = 0;// clear data flag.

        // Set maximum step for counting.
        // My motor spin 360 degree with 1792 steps
        // FYI. I use 2204 260kV gimbal motor.
        step_max = i2cbuf[2] << 8;
        step_max |= i2cbuf[1];
        
        //speed = (i2cbuf[4] << 8) | i2cbuf[3];
        speed = i2cbuf[3];
        speed_ramp = 61;
        
        switch (i2cbuf[0]) {
          case 0x40:// Step backward
            OCR1B = pgm_read_byte(&lut[sinU]);
            OCR1A = pgm_read_byte(&lut[sinV]);
            OCR1D = pgm_read_byte(&lut[sinW]);
            main_fsm = 1;
            initTimer0(speed);
            break;

          case 0x80:// Step forward
            OCR1B = pgm_read_byte(&lut[sinU]);
            OCR1A = pgm_read_byte(&lut[sinV]);
            OCR1D = pgm_read_byte(&lut[sinW]);
            main_fsm = 2;
            initTimer0(speed);
            break;

          case 0x20:// Speed forward
            OCR1B = pgm_read_byte(&lut[sinU]);
            OCR1A = pgm_read_byte(&lut[sinV]);
            OCR1D = pgm_read_byte(&lut[sinW]);
            main_fsm = 3;
            initTimer0(speed_ramp);
            break;

          case 0x10:// Speed backward
            OCR1B = pgm_read_byte(&lut[sinU]);
            OCR1A = pgm_read_byte(&lut[sinV]);
            OCR1D = pgm_read_byte(&lut[sinW]);
            main_fsm = 4;
            initTimer0(speed_ramp);
            break;
        }
      }

      break;

    case 8:// go back to main_fsm = 0
      deinitTimer0();
      PORTA &= ~(1 << 4);
      main_fsm = 0;
      step_cnt = 0;
      speed_ramp = 32;
      ramp_fsm = 0;
      
      break;
  }

   if((main_fsm == 3) || (main_fsm == 4)){
     if(ramp_fsm == 0){
          ramp_cnt++;
          if(ramp_cnt == 6000){
           ramp_cnt = 0;
            PORTA ^= (1 << 4);
            speed_ramp--;
            if(speed_ramp <= speed){
              speed_ramp = speed;
              ramp_fsm = 1;
            }
            initTimer0(speed_ramp);
            //delay(50);
        }
    }
   }
}
