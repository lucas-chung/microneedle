#include "ad5940_port.h"
#include "board_pins.h"
#include <SPI.h>

static SPISettings g_ad5941SpiSettings(1000000, MSBFIRST, SPI_MODE0);
static volatile uint32_t g_mcuIntFlag = 0;

static void IRAM_ATTR ad5941IntISR() {
  g_mcuIntFlag = 1;
}

void AD5940_Port_Init(void) {
  pinMode(PIN_AD5941_CS, OUTPUT);
  digitalWrite(PIN_AD5941_CS, HIGH);

  pinMode(PIN_AD5941_GP0, INPUT);
  if (PIN_AD5941_GP1 >= 0) {
    pinMode(PIN_AD5941_GP1, INPUT);
  }

  pinMode(PIN_MUL_A0, OUTPUT);
  pinMode(PIN_MUL_A1, OUTPUT);
  pinMode(PIN_MUL_EN, OUTPUT);

  // Select WE1 by default. ADG704 EN=1 enables the selected switch.
  digitalWrite(PIN_MUL_EN, LOW);
  digitalWrite(PIN_MUL_A0, LOW);
  digitalWrite(PIN_MUL_A1, LOW);
  delay(2);
  digitalWrite(PIN_MUL_EN, HIGH);
  delay(10);

  // ESP32 Arduino: SPI.begin(SCLK, MISO, MOSI)
  SPI.begin(PIN_AD5941_SCLK, PIN_AD5941_MISO, PIN_AD5941_MOSI);

  attachInterrupt(digitalPinToInterrupt(PIN_AD5941_GP0), ad5941IntISR, FALLING);
}

void Board_SelectWE1(void) {
  digitalWrite(PIN_MUL_EN, LOW);
  digitalWrite(PIN_MUL_A0, LOW);
  digitalWrite(PIN_MUL_A1, LOW);
  delay(2);
  digitalWrite(PIN_MUL_EN, HIGH);
  delay(10);
}

void AD5940_CsClr(void) {
  digitalWrite(PIN_AD5941_CS, LOW);
}

void AD5940_CsSet(void) {
  digitalWrite(PIN_AD5941_CS, HIGH);
}

void AD5940_RstClr(void) {
}

void AD5940_RstSet(void) {
}

void AD5940_Delay10us(uint32_t time) {
  delayMicroseconds(time * 10);
}

void AD5940_ReadWriteNBytes(unsigned char *pSendBuffer,
                            unsigned char *pRecvBuff,
                            unsigned long length) {
  SPI.beginTransaction(g_ad5941SpiSettings);

  for (unsigned long i = 0; i < length; i++) {
    uint8_t tx = pSendBuffer ? pSendBuffer[i] : 0xFF;
    uint8_t rx = SPI.transfer(tx);

    if (pRecvBuff) {
      pRecvBuff[i] = rx;
    }
  }

  SPI.endTransaction();
}

uint32_t AD5940_GetMCUIntFlag(void) {
  return g_mcuIntFlag;
}

uint32_t AD5940_ClrMCUIntFlag(void) {
  g_mcuIntFlag = 0;
  return 1;
}
