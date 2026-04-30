// ============================================================================
// Setup for the TTGO T-Display with ST7789V display
// ============================================================================
#define USER_SETUP_ID 25

// Driver selection
#define ST7789_DRIVER

// Display has a bidirectional SDA pin
#define TFT_SDA_READ

// Display resolution
#define TFT_WIDTH  135
#define TFT_HEIGHT 240

// Library will add offsets required
#define CGRAM_OFFSET

// SPI pin definitions for TTGO T-Display
#define TFT_MOSI    19   // GPIO19 - SPI MOSI (DATA)
#define TFT_SCLK    18   // GPIO18 - SPI CLK (CLOCK)
#define TFT_CS      5    // GPIO5  - Chip Select
#define TFT_DC      16   // GPIO16 - Data/Command
#define TFT_RST     23   // GPIO23 - Reset
#define TFT_BL      4    // GPIO4  - Backlight control

// Backlight control (HIGH = ON, LOW = OFF)
#define TFT_BACKLIGHT_ON HIGH

// Color order for TTGO T-Display
#define TFT_RGB_ORDER TFT_BGR  // Blue-Green-Red

// Enable color inversion for vivid colors on ST7789V
// Without this, colors appear washed out / pale
#define TFT_INVERSION_ON

// Font options
#define LOAD_GLCD    // Load GLCD 5x7 font
#define LOAD_FONT2   // Load Font 2 
#define LOAD_FONT4   // Load Font 4
#define LOAD_FONT6   // Load Font 6
#define LOAD_FONT7   // Load Font 7
#define LOAD_FONT8   // Load Font 8
#define LOAD_GFXFF   // Load GFX Free Fonts

// Use smooth fonts
#define SMOOTH_FONT

// SPI frequency settings
#define SPI_FREQUENCY   40000000   // 40 MHz for write operations
#define SPI_READ_FREQUENCY  6000000 // 6 MHz max for ST7789V read operations

// ============================================================================
// End of TTGO T-Display configuration
// ============================================================================
