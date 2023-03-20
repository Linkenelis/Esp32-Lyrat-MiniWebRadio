// Digital I/O used
/*#define VS1053_CS     2
#define VS1053_DCS     4
#define VS1053_DREQ   32
#define TFT_CS        22
#define TFT_DC        21
#define TFT_BL        0
#define TP_IRQ        33
#define TP_CS         15*/
//#define SD_MMC_D0      2  // cannot be changed
//#define SD_MMC_CLK    14  // cannot be changed
//#define SD_MMC_CMD    15  // cannot be changed
#define SD_CS         13    // 5
#define VS1053_MOSI   15  // SD
#define VS1053_MISO   2  // SD
#define VS1053_SCK    14  //SD
//#define IR_PIN        3
/*#define SPI_MOSI      13  // TFT and TP (HSPI)
#define SPI_MISO      12  // TFT and TP (HSPI)
#define SPI_SCK       14  // TFT and TP (HSPI)
#define VS1053_MOSI   23  // VS1053     (VSPI)
#define VS1053_MISO   19  // VS1053     (VSPI)
#define VS1053_SCK    18  // VS1053     (VSPI)*/
#define I2S_DOUT      26    //26    //25  // DAC: I2S_DIN
#define I2S_BCLK      5
#define I2S_LRC       25    //26
#define I2S_MCLK       0  // mostly not used
#define I2S_ASDOUT      35  // mostly not used

// I2C GPIOs
#define IIC_CLK       23
#define IIC_DATA      18

// LyraT_buttons and specials
#define GPIO_PA_EN    21
#define SET           32
#define REC           36
#define MODE          39
#define PLAY          33
#define VOLUP         27
#define VOLDWN        13
#define BOOT          2