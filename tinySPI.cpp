/*-------------------------------------------------------------------------*
 * tinySPI.h - Arduino hardware SPI master library for ATtiny24/44/84,     *
 * ATtiny25/45/85, and Attiny2313/4313.                                    *
 *                                                                         *
 * Original version of tinyISP by Jack Christensen 24Oct2013               *
 *                                                                         *
 * Added support for Attiny24/25, and Attiny2313/4313                      *
 * by Leonardo Miliani 28Nov2014                                           *                          
 *                                                                         *
 * CC BY-SA-NC:                                                            *
 * This work is licensed under the Creative Commons Attribution-           *
 * ShareAlike- Not Commercial 4.0 Unported License. To view a copy of this *
 * license, visit                                                          *
 * http://creativecommons.org/licenses/by-sa/4.0/ or send a                *
 * letter to Creative Commons, 171 Second Street, Suite 300,               *
 * San Francisco, California, 94105, USA.                                  *
 *-------------------------------------------------------------------------*/

#include "tinySPI.h"


#ifdef SPDR //Then we have hardware SPI, let's use it:


SPIClass SPI;

uint8_t SPIClass::initialized = 0;
uint8_t SPIClass::interruptMode = 0;
uint8_t SPIClass::interruptMask = 0;
uint8_t SPIClass::interruptSave = 0;
#ifdef SPI_TRANSACTION_MISMATCH_LED
uint8_t SPIClass::inTransactionFlag = 0;
#endif

void SPIClass::begin()
{
  uint8_t sreg = SREG;
  noInterrupts(); // Protect from a scheduler and prevent transactionBegin
  if (!initialized) {
    // Set SS to high so a connected chip will be "deselected" by default
    uint8_t port = digitalPinToPort(SS);
    uint8_t bit = digitalPinToBitMask(SS);
    volatile uint8_t *reg = portModeRegister(port);

    // if the SS pin is not already configured as an output
    // then set it high (to enable the internal pull-up resistor)
    if(!(*reg & bit)){
      digitalWrite(SS, HIGH);
    }

    // When the SS pin is set as OUTPUT, it can be used as
    // a general purpose output port (it doesn't influence
    // SPI operations).
    pinMode(SS, OUTPUT);

    // Warning: if the SS pin ever becomes a LOW INPUT then SPI
    // automatically switches to Slave, so the data direction of
    // the SS pin MUST be kept as OUTPUT.
    SPCR |= _BV(MSTR);
    SPCR |= _BV(SPE);

    // Set direction register for SCK and MOSI pin.
    // MISO pin automatically overrides to INPUT.
    // By doing this AFTER enabling SPI, we avoid accidentally
    // clocking in a single bit since the lines go directly
    // from "input" to SPI control.
    // http://code.google.com/p/arduino/issues/detail?id=888
    pinMode(SCK, OUTPUT);
    pinMode(MOSI, OUTPUT);
  }
  initialized++; // reference count
  SREG = sreg;
}

void SPIClass::end() {
  uint8_t sreg = SREG;
  noInterrupts(); // Protect from a scheduler and prevent transactionBegin
  // Decrease the reference counter
  if (initialized)
    initialized--;
  // If there are no more references disable SPI
  if (!initialized) {
    SPCR &= ~_BV(SPE);
    interruptMode = 0;
    #ifdef SPI_TRANSACTION_MISMATCH_LED
    inTransactionFlag = 0;
    #endif
  }
  SREG = sreg;
}

// mapping of interrupt numbers to bits within SPI_AVR_EIMSK
#if defined(__AVR_ATmega32U4__)
  #define SPI_INT0_MASK  (1<<INT0)
  #define SPI_INT1_MASK  (1<<INT1)
  #define SPI_INT2_MASK  (1<<INT2)
  #define SPI_INT3_MASK  (1<<INT3)
  #define SPI_INT4_MASK  (1<<INT6)
#elif defined(__AVR_AT90USB646__) || defined(__AVR_AT90USB1286__)
  #define SPI_INT0_MASK  (1<<INT0)
  #define SPI_INT1_MASK  (1<<INT1)
  #define SPI_INT2_MASK  (1<<INT2)
  #define SPI_INT3_MASK  (1<<INT3)
  #define SPI_INT4_MASK  (1<<INT4)
  #define SPI_INT5_MASK  (1<<INT5)
  #define SPI_INT6_MASK  (1<<INT6)
  #define SPI_INT7_MASK  (1<<INT7)
#elif defined(EICRA) && defined(EICRB) && defined(EIMSK)
  #define SPI_INT0_MASK  (1<<INT4)
  #define SPI_INT1_MASK  (1<<INT5)
  #define SPI_INT2_MASK  (1<<INT0)
  #define SPI_INT3_MASK  (1<<INT1)
  #define SPI_INT4_MASK  (1<<INT2)
  #define SPI_INT5_MASK  (1<<INT3)
  #define SPI_INT6_MASK  (1<<INT6)
  #define SPI_INT7_MASK  (1<<INT7)
#else
  #ifdef INT0
  #define SPI_INT0_MASK  (1<<INT0)
  #endif
  #ifdef INT1
  #define SPI_INT1_MASK  (1<<INT1)
  #endif
  #ifdef INT2
  #define SPI_INT2_MASK  (1<<INT2)
  #endif
#endif

void SPIClass::usingInterrupt(uint8_t interruptNumber)
{
  uint8_t mask = 0;
  uint8_t sreg = SREG;
  noInterrupts(); // Protect from a scheduler and prevent transactionBegin
  switch (interruptNumber) {
  #ifdef SPI_INT0_MASK
  case 0: mask = SPI_INT0_MASK; break;
  #endif
  #ifdef SPI_INT1_MASK
  case 1: mask = SPI_INT1_MASK; break;
  #endif
  #ifdef SPI_INT2_MASK
  case 2: mask = SPI_INT2_MASK; break;
  #endif
  #ifdef SPI_INT3_MASK
  case 3: mask = SPI_INT3_MASK; break;
  #endif
  #ifdef SPI_INT4_MASK
  case 4: mask = SPI_INT4_MASK; break;
  #endif
  #ifdef SPI_INT5_MASK
  case 5: mask = SPI_INT5_MASK; break;
  #endif
  #ifdef SPI_INT6_MASK
  case 6: mask = SPI_INT6_MASK; break;
  #endif
  #ifdef SPI_INT7_MASK
  case 7: mask = SPI_INT7_MASK; break;
  #endif
  default:
    interruptMode = 2;
    break;
  }
  interruptMask |= mask;
  if (!interruptMode)
    interruptMode = 1;
  SREG = sreg;
}

void SPIClass::notUsingInterrupt(uint8_t interruptNumber)
{
  // Once in mode 2 we can't go back to 0 without a proper reference count
  if (interruptMode == 2)
    return;
  uint8_t mask = 0;
  uint8_t sreg = SREG;
  noInterrupts(); // Protect from a scheduler and prevent transactionBegin
  switch (interruptNumber) {
  #ifdef SPI_INT0_MASK
  case 0: mask = SPI_INT0_MASK; break;
  #endif
  #ifdef SPI_INT1_MASK
  case 1: mask = SPI_INT1_MASK; break;
  #endif
  #ifdef SPI_INT2_MASK
  case 2: mask = SPI_INT2_MASK; break;
  #endif
  #ifdef SPI_INT3_MASK
  case 3: mask = SPI_INT3_MASK; break;
  #endif
  #ifdef SPI_INT4_MASK
  case 4: mask = SPI_INT4_MASK; break;
  #endif
  #ifdef SPI_INT5_MASK
  case 5: mask = SPI_INT5_MASK; break;
  #endif
  #ifdef SPI_INT6_MASK
  case 6: mask = SPI_INT6_MASK; break;
  #endif
  #ifdef SPI_INT7_MASK
  case 7: mask = SPI_INT7_MASK; break;
  #endif
  default:
    break;
    // this case can't be reached
  }
  interruptMask &= ~mask;
  if (!interruptMask)
    interruptMode = 0;
  SREG = sreg;
}

#else 
#ifdef USICR


  static void begin(); DONE
  inline static uint8_t transfer(uint8_t data) DONE
  inline static uint16_t transfer16(uint16_t data) DONE
  inline static void transfer(void *buf, size_t count) DONE
  static void end(); DONE
  inline static void beginTransaction(SPISettings settings)
  static void usingInterrupt(uint8_t interruptNumber); 
  static void notUsingInterrupt(uint8_t interruptNumber);
  inline static void beginTransaction(SPISettings settings)
  inline static void endTransaction(void)
  //inline static void setBitOrder(uint8_t bitOrder)
  inline static void setDataMode(uint8_t dataMode)
  //inline static void setClockDivider(uint8_t clockDiv)
  inline static void attachInterrupt() { SPCR |= _BV(SPIE); }
  inline static void detachInterrupt() { SPCR &= ~_BV(SPIE); }
tinySPI::tinySPI() 
{
}
 
void tinySPI::begin(void)
{
    USICR &= ~(_BV(USISIE) | _BV(USIOIE) | _BV(USIWM1));
    USICR |= _BV(USIWM0) | _BV(USICS1) | _BV(USICLK);
    USI_DDR_PORT |= _BV(USCK_DD_PIN);   //set the USCK pin as output
    USI_DDR_PORT |= _BV(DO_DD_PIN);     //set the DO pin as output
    USI_DDR_PORT &= ~_BV(DI_DD_PIN);    //set the DI pin as input
}

void tinySPI::setDataMode(uint8_t spiDataMode)
{
    if (spiDataMode == SPI_MODE1)
        USICR |= _BV(USICS0);
    else
        USICR &= ~_BV(USICS0);
}

uint8_t tinySPI::transfer(uint8_t spiData)
{
    USIDR = spiData;
    USISR = _BV(USIOIF);                //clear counter and counter overflow interrupt flag
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { //ensure a consistent clock period
        while ( !(USISR & _BV(USIOIF)) ) USICR |= _BV(USITC);
    }
    return USIDR;
}

uint16_t tinySPI::transfer16(uint16_t data) {
 union { uint16_t val; struct { uint8_t lsb; uint8_t msb; }; } in, out;
 in.val = data;
 USIDR = in.msb;
 USISR = _BV(USIOIF);                //clear counter and counter overflow interrupt flag
 ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { //ensure a consistent clock period
  while ( !(USISR & _BV(USIOIF)) ) USICR |= _BV(USITC);
 }
 out.msb = USIDR; 
 USIDR = in.lsb;
 USISR = _BV(USIOIF);                //clear counter and counter overflow interrupt flag
 ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { //ensure a consistent clock period
  while ( !(USISR & _BV(USIOIF)) ) USICR |= _BV(USITC);
 }
 out.lsb = USIDR;
 return out.val;
}

uint16_t tinySPI::transfer(void *buf, size_t count) {
 if (count == 0) return;
 uint8_t *p = (uint8_t *)buf;
 USIDR = *p;
 while (--count > 0) {
  uint8_t out = *(p + 1);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { //ensure a consistent clock period
   while ( !(USISR & _BV(USIOIF)) ) USICR |= _BV(USITC);
  }
  uint8_t in = USIDR;
  USIDR = out;
  *p++ = in;
 }
 ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { //ensure a consistent clock period
  while ( !(USISR & _BV(USIOIF)) ) USICR |= _BV(USITC);
 }
 *p = USIDR;
}


void tinySPI::end(void)
{
    USICR &= ~(_BV(USIWM1) | _BV(USIWM0));
}

tinySPI SPI = tinySPI();                //instantiate a tinySPI object

#endif
#endif
