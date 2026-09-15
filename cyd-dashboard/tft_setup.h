// TFT_eSPI user setup for the CYD family boards.
//
// TFT_eSPI auto-detects a tft_setup.h in the sketch folder (see TFT_eSPI.h)
// and uses it instead of User_Setup_Select.h, so no manual edits to the
// installed TFT_eSPI library are needed - the build is identical locally and
// in CI.
//
// Two boards are supported; select with -DCYD_E32R40T in the compile flags:
//   default (2.8" ESP32-2432S028R):
//     ILI9341 320x240; TFT on HSPI: CS=15 DC=2 RST=4 SCK=14 MOSI=13 MISO=12
//     Touch on VSPI: T_CS=33 T_IRQ=36 T_DIN=32 T_DOUT=39 T_CLK=25
//     Backlight: GPIO 21 (active high)
//   CYD_E32R40T (4.0" E32R40T):
//     ST7796 480x320; TFT on HSPI, same SPI pins, RST tied to board EN
//     Touch SHARES the TFT bus (T_CS=33 T_IRQ=36); backlight on GPIO 27.
//     The 360x240 logical UI is uniformly scaled x4/3 onto the panel by the
//     tft wrapper (SCALEX/SCALEY in cyd-dashboard.ino) and uses FreeFonts.

#ifdef CYD_E32R40T

#define ST7796_DRIVER
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

#define USE_HSPI_PORT
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1      // panel reset shares the board's EN line

#define TFT_BL   27
#define TFT_BACKLIGHT_ON HIGH

// XPT2046 rides the TFT's HSPI bus on this board (no separate VSPI wiring).
#define TOUCH_CS  33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 13
#define TOUCH_MISO 12
#define TOUCH_CLK  14
#define CYD_TP_SPI_BUS HSPI

#define SPI_TOUCH_FREQUENCY 2500000

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_GFXFF   // FreeFonts - smooth text on the larger panel

#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  16000000

#else

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
// GPIO 4 really is the panel reset on this unit. The esp32 core's variant file
// claims GPIO 4 is the red LED (CYD_LED_RED), but on this board revision the
// red channel is on GPIO 22 - verified by driving each free pin in turn (see
// the LED pin defines in cyd-dashboard.ino).
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

#endif // CYD_E32R40T
