// TFT_eSPI user setup for the ESP32-2432S028R "CYD" board.
//
// TFT_eSPI auto-detects a tft_setup.h in the sketch folder (see TFT_eSPI.h)
// and uses it instead of User_Setup_Select.h, so no manual edits to the
// installed TFT_eSPI library are needed - the build is identical locally and
// in CI. Verified against the esp32 core's variant for jczn_2432s028r:
//   TFT on HSPI:  CS=15  DC=2  RST=4  SCK=14  MOSI=13  MISO=12
//   Touch on VSPI: T_CS=33  T_IRQ=36  T_DIN=32  T_DOUT=39  T_CLK=25
//   Backlight: GPIO 21  (active high)

#define ILI9341_DRIVER

// Force TFT_eSPI onto the HSPI (SPI2) peripheral so it never shares hardware
// with the touch controller's separate SPIClass(VSPI) instance (otherwise the
// TFT defaults to VSPI and corrupts touch reads on this board).
#define USE_HSPI_PORT

// ---- TFT SPI (HSPI) ----
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4

// ---- Backlight ----
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH   // active-high backlight on this board

// ---- Touch controller (XPT2046) on VSPI, separate from the TFT SPI ----
#define TOUCH_CS  33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
#define CYD_TP_SPI_BUS VSPI

#define SPI_TOUCH_FREQUENCY 2500000

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6

#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  16000000
