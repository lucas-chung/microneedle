#ifndef BOARD_PINS_H
#define BOARD_PINS_H

// ===== Shared SPI bus: AD5941 + TF card =====
// Final verified wiring:
// IO5  -> AD5941 SCLK
// IO6  -> AD5941 MOSI
// IO7  -> AD5941 MISO
static const int PIN_SPI_SCLK  = 5;
static const int PIN_SPI_MOSI  = 6;
static const int PIN_SPI_MISO  = 7;

// ===== AD5941 =====
// IO10 -> AD5941 CS
static const int PIN_AD5941_SCLK  = PIN_SPI_SCLK;
static const int PIN_AD5941_MOSI  = PIN_SPI_MOSI;
static const int PIN_AD5941_MISO  = PIN_SPI_MISO;
static const int PIN_AD5941_CS    = 10;

// ===== TF card =====
// IO18 -> SPI_TF_CS
static const int PIN_SD_CS        = 18;

// ===== AD5941 GPIO =====
// IO0 -> AD5941 GPIO0 interrupt output
// IO4 -> AD5941 GPIO1 interrupt output
static const int PIN_AD5941_GP0   = 0;
static const int PIN_AD5941_GP1   = 4;

// ===== ADG704 MUX =====
// IO1  -> MUL_EN
// IO3  -> MUL_A0
// IO19 -> MUL_A1
static const int PIN_MUL_EN       = 1;
static const int PIN_MUL_A0       = 3;
static const int PIN_MUL_A1       = 19;

#endif
