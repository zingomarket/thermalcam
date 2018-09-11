//this is better with included zip package that has all needed libraries in it for testing.
//most updated version of thermal_cam file can replace the one in zip file
//this is a work in process. 16x16 subsample seems to work ok, trying to verify 32x32 and add up to 128 by 128 for now
//some changes may be needed to have includes location changes also spixx.h just uses libray in file change back to <spi.h>
//this is the only file where code changes are, so just replace old file with this one 
//and change libary include file locations
//32x32 might need work. will need to first increase color table to be sure
//located here on github :https://github.com/jamesdanielv/thermalcam/edit/master/thermal_cam.ino
/***************************************************************************
  This is a library for the AMG88xx GridEYE 8x8 IR camera
  This sketch makes a 64 pixel thermal camera with the GridEYE sensor
  and a 128x128 tft screen https://www.adafruit.com/product/2088
  Designed specifically to work with the Adafruit AMG88 breakout
  ----> http://www.adafruit.com/products/3538
  These sensors use I2C to communicate. The device's I2C address is 0x69
  Adafruit invests time and resources providing this open source code,
  please support Adafruit andopen-source hardware by purchasing products
  from Adafruit!
  Written by Dean Miller for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ***************************************************************************/
 //this file and files included have been optimized and modified. floats are only used for temp calc. and for mapping to int for colors maps
//this version modified and optimized by james villeneuve
// all text above must be included in any redistribution
// ***************************************************************************/
#include "Adafruit_GFX.h"    // Core graphics library
#include "Adafruit_ST7735.h" // Hardware-specific library
#include "SPIxx.h" //built in library should be SPI.h 

#include "Adafruit_SPITFT_Macros.h"
#include <Wire.h>
#include "Adafruit_AMG88xx.h"
#include <avr/pgmspace.h>

//these 3 will need to be redefined in Adafruit_ST77xx.ccp file for small amount of time. seems some slow pixel writing on arduino otherwise!
#define TFT_CS     10 //chip select pin for the TFT screen
#define TFT_RST    9  // you can also connect this to the Arduino reset
                      // in which case, set this #define pin to 0!
#define TFT_DC     8
#define    displayPixelWidth  tft.width() /8  //allows values to be hardcoded
#define    displayPixelHeight   tft.height() / 8//these determine pixel block size. 
#define autorange true //this adjust temp range over the color range of the screen experimental
#define optics_correction  true //experimental
#define threshold 0 //any value below this range will be treated as lowest value. a tool for noise reduction
#define enhancedDetail  true   //experimental. only works  if >16x16 resolution. this has some extra steps in pixel processing that enhances resolution detail.see smaller objects from farther away. can also increase noise.
#define interlaced  false //his is original configuration it eliminates sliding effect, but might also have lcd refresh happen too quickly and cause different brightness levels per 8x8 or 16x16 grid
//low range of the sensor (this will be blue on the screen)


// 0 no optimse |1 pixels only written whe color changed| 2 pixels also optimized for most changed ones first (deals with noise issues)
#define optimize  2 //
//interpolate 4 is resolution of display. it gets rid of blockiness effect by applying blending, and sub samping 2x2 more
#define interpolatemode 4 //can be 0-3 so far 0=8x8 1=16x16 2=32x32 3=64x64
//experimental subpixelcoloroptimized=3 more only works on interpolate mode 3, for interpolate mode 1,0 use subpixelcoloroptimized=0
#define noisefilter 5 //0 is off.  this means temp variation needs to be greater than this for data to be sent to lcd, also you could program interupt of amg8833 for this as well

//mirror display! look in Adafruit_AMG88xx.cpp file for a setting called #define AMG88xx_PIXEL_MIRROR //this mirrors image from left to right to right to left for sensor, such as when amg8833 device is pointing away from display. 
#define show_temp_readout false //this shows center dot, and right below the temp for that dot. if false wont show anything
#define temp_Fahrenheit true //we convert output temp to F, otherwise it is c
#define showcolorbar false  //this shows color range and bar. more useful for autorange
#define colorMode 21 //can be 0=64 color adafruit, 1=256 color map 0,1 use same 256 color table space,2=1024 colors , or color=21 for alternate 1024 color map, 22 for mostl black and white with high temp in color, 23 another color mode 
//there is an error in the way this code works with 1.8.5 and mode 0,1 with subpixelcoloroptimized and spi_optimized_st77xx true, so set spi_optimized_st77xx false (this does not matter on subpixelcoloroptimized > 0 (multi pixel writes)
#define spi_optimized_st77xx true //false if using normal driver//these files are upaded and specific for this code needed st77xx,h, st77xx.cpp need downloaded from https://github.com/jamesdanielv/thermalcam/blob/master/colorgenerator
#define subpixelcoloroptimized 1 //0= normal,-1 lcd write removed to look at rest of loop time. higher resolution modes use more pixel writes modes automatically beyond interpolate 1.kept here for testing 
#define spi_buffer_local false //buffer not interwoven in code yet. only works 64x64. so it stops at every block update and makes sure to drain buffer, filling buffer draining buffer and pixel show is slow unless data unloaded during processing. working on it
#define spi_safety_checks false //something similar is in display drive. if set to true does safety checks for various speeds. it is safer, but has a penalty in performance
//subpixelcoloroptimized only work with custom st77xx files currently and are only enabled if 32x32 or 64x64 subsampled enabled.

//*************************************           no need to look below here. look up above for things to change!*******************************************************
//*************************************           no need to look below here. look up above for things to change!*******************************************************
//*************************************           no need to look below here. look up above for things to change!*******************************************************
//*************************************           no need to look below here. look up above for things to change!*******************************************************
//*************************************           no need to look below here. look up above for things to change!*******************************************************
//*************************************           no need to look below here. look up above for things to change!*******************************************************
//*************************************           no need to look below here. look up above for things to change!*******************************************************
//*************************************           no need to look below here. look up above for things to change!*******************************************************
#if spi_optimized_st77xx == true
#define fillRectFast tft.fillRectFast
#else
#define fillRectFast tft.fillRect
#endif
#if autorange == false  //leave this value alone. it is part of code. setting is above
#define MINTEMP 22
#define MAXTEMP 34
#else
int MINTEMP =24;
int MAXTEMP =34; //we turn this into int value that can be adjusted
#endif
#if colorMode == 0
const PROGMEM uint16_t camColors[] =  {0x480F,
0x400F,0x400F,0x400F,0x4010,0x3810,0x3810,0x3810,0x3810,0x3010,0x3010,
0x3010,0x2810,0x2810,0x2810,0x2810,0x2010,0x2010,0x2010,0x1810,0x1810,
0x1811,0x1811,0x1011,0x1011,0x1011,0x0811,0x0811,0x0811,0x0011,0x0011,
0x0011,0x0011,0x0011,0x0031,0x0031,0x0051,0x0072,0x0072,0x0092,0x00B2,
0x00B2,0x00D2,0x00F2,0x00F2,0x0112,0x0132,0x0152,0x0152,0x0172,0x0192,
0x0192,0x01B2,0x01D2,0x01F3,0x01F3,0x0213,0x0233,0x0253,0x0253,0x0273,
0x0293,0x02B3,0x02D3,0x02D3,0x02F3,0x0313,0x0333,0x0333,0x0353,0x0373,
0x0394,0x03B4,0x03D4,0x03D4,0x03F4,0x0414,0x0434,0x0454,0x0474,0x0474,
0x0494,0x04B4,0x04D4,0x04F4,0x0514,0x0534,0x0534,0x0554,0x0554,0x0574,
0x0574,0x0573,0x0573,0x0573,0x0572,0x0572,0x0572,0x0571,0x0591,0x0591,
0x0590,0x0590,0x058F,0x058F,0x058F,0x058E,0x05AE,0x05AE,0x05AD,0x05AD,
0x05AD,0x05AC,0x05AC,0x05AB,0x05CB,0x05CB,0x05CA,0x05CA,0x05CA,0x05C9,
0x05C9,0x05C8,0x05E8,0x05E8,0x05E7,0x05E7,0x05E6,0x05E6,0x05E6,0x05E5,
0x05E5,0x0604,0x0604,0x0604,0x0603,0x0603,0x0602,0x0602,0x0601,0x0621,
0x0621,0x0620,0x0620,0x0620,0x0620,0x0E20,0x0E20,0x0E40,0x1640,0x1640,
0x1E40,0x1E40,0x2640,0x2640,0x2E40,0x2E60,0x3660,0x3660,0x3E60,0x3E60,
0x3E60,0x4660,0x4660,0x4E60,0x4E80,0x5680,0x5680,0x5E80,0x5E80,0x6680,
0x6680,0x6E80,0x6EA0,0x76A0,0x76A0,0x7EA0,0x7EA0,0x86A0,0x86A0,0x8EA0,
0x8EC0,0x96C0,0x96C0,0x9EC0,0x9EC0,0xA6C0,0xAEC0,0xAEC0,0xB6E0,0xB6E0,
0xBEE0,0xBEE0,0xC6E0,0xC6E0,0xCEE0,0xCEE0,0xD6E0,0xD700,0xDF00,0xDEE0,
0xDEC0,0xDEA0,0xDE80,0xDE80,0xE660,0xE640,0xE620,0xE600,0xE5E0,0xE5C0,
0xE5A0,0xE580,0xE560,0xE540,0xE520,0xE500,0xE4E0,0xE4C0,0xE4A0,0xE480,
0xE460,0xEC40,0xEC20,0xEC00,0xEBE0,0xEBC0,0xEBA0,0xEB80,0xEB60,0xEB40,
0xEB20,0xEB00,0xEAE0,0xEAC0,0xEAA0,0xEA80,0xEA60,0xEA40,0xF220,0xF200,
0xF1E0,0xF1C0,0xF1A0,0xF180,0xF160,0xF140,0xF100,0xF0E0,0xF0C0,0xF0A0,
0xF080,0xF060,0xF040,0xF020,0xF800,};
#endif

#if colorMode == 1
const PROGMEM uint16_t camColors[] =  
{0x0003,0x0004,0x0004,0x0005,0x0006,0x0006,0x0007,0x0027,0x0028,
0x0028,0x0029,0x002a,0x002a,0x002b,0x002b,0x002c,0x002d,0x002d,
0x002e,0x002e,0x002f,0x004f,0x0050,0x0051,0x0051,0x0052,0x0052,
0x0053,0x0053,0x0054,0x0055,0x0055,0x0056,0x0056,0x0057,0x0057,
0x0078,0x0079,0x0079,0x007a,0x007a,0x007b,0x007c,0x007c,0x007d,
0x007d,0x007e,0x007e,0x007f,0x009f,0x00bf,0x00de,0x00fe,0x011d,
0x013d,0x015c,0x017c,0x019b,0x01ba,0x01fa,0x0219,0x0239,0x0258,
0x0278,0x0297,0x02b7,0x02d6,0x02f6,0x0315,0x0334,0x0374,0x0393,
0x03b3,0x03d2,0x03f2,0x0411,0x0431,0x0450,0x0470,0x048f,0x04ae,
0x04ce,0x050d,0x052d,0x054c,0x056c,0x058b,0x05ab,0x05ca,0x05ea,
0x0609,0x0628,0x0648,0x0687,0x06a7,0x06c6,0x06e6,0x0705,0x0725,
0x0744,0x0764,0x0783,0x07a2,0x07c2,0x07e1,0x07e1,0x0fe1,0x0fe1,
0x17e1,0x17e1,0x1fe1,0x1fe1,0x27e1,0x2fe1,0x2fe1,0x37e1,0x37e1,
0x3fe1,0x3fc1,0x47c1,0x47c1,0x4fc1,0x4fc1,0x57c1,0x5fc1,0x5fc1,
0x67c1,0x67c1,0x6fc1,0x6fc1,0x77c1,0x77c1,0x7fa0,0x7fa0,0x87a0,
0x87a0,0x8fa0,0x97a0,0x97a0,0x9fa0,0x9fa0,0xa7a0,0xa7a0,0xafa0,
0xafa0,0xb7a0,0xb7a0,0xbf80,0xc780,0xc780,0xcf80,0xcf80,0xd780,
0xd780,0xdf80,0xdf80,0xe780,0xe780,0xef80,0xf780,0xf760,0xf760,
0xf760,0xf740,0xf740,0xf740,0xf721,0xf721,0xf721,0xf701,0xf701,
0xf701,0xf6e2,0xf6e2,0xf6e2,0xf6c2,0xf6c2,0xf6c2,0xf6a2,0xf6a3,
0xf6a3,0xf683,0xf683,0xf683,0xf663,0xf664,0xf644,0xfe44,0xfe44,
0xfe24,0xfe24,0xfe25,0xfe05,0xfe05,0xfe05,0xfde5,0xfde5,0xfde5,
0xfdc6,0xfdc6,0xfdc6,0xfda6,0xfda6,0xfda6,0xfd87,0xfd87,0xfd87,
0xfd67,0xfd67,0xfd67,0xfd47,0xfd48,0xfd48,0xfd28,0xfd28,0xfd08,
0xfd08,0xfce8,0xfcc7,0xfca7,0xfca7,0xfc87,0xfc67,0xfc47,0xfc47,
0xfc26,0xfc06,0xfbe6,0xfbe6,0xfbc6,0xfba6,0xfb85,0xfb85,0xfb65,
0xfb45,0xfb25,0xfb25,0xfb05,0xfae4,0xfac4,0xfac4,0xfaa4,0xfa84,
0xfa64,0xfa63,0xfa43,0xfa23,0xfa03,0xfa03,0xf9e3,0xf9c3,0xf9a2,
0xf9a2,0xf982,0xf962,0xf942};
#endif
#if colorMode == 2
const PROGMEM uint16_t camColors[] =
{0x002e,0x002e,0x002e,0x002f,0x002f,0x002f,0x002f,0x002f,0x002f,0x004f,0x002f,0x0050,0x0030,
0x0050,0x0030,0x0050,0x0050,0x0051,0x0051,0x0051,0x0051,0x0051,0x0051,0x0051,0x0052,0x0052,
0x0052,0x0052,0x0052,0x0052,0x0052,0x0053,0x0053,0x0053,0x0053,0x0053,0x0053,0x0053,0x0054,
0x0054,0x0054,0x0054,0x0054,0x0054,0x0054,0x0055,0x0055,0x0055,0x0055,0x0055,0x0055,0x0055,
0x0056,0x0056,0x0056,0x0056,0x0056,0x0056,0x0056,0x0057,0x0057,0x0057,0x0057,0x0057,0x0057,
0x0057,0x0058,0x0078,0x0058,0x0078,0x0058,0x0078,0x0058,0x0079,0x0059,0x0079,0x0079,0x0079,
0x0079,0x007a,0x007a,0x007a,0x007a,0x007a,0x007a,0x007a,0x007a,0x007b,0x007b,0x007b,0x007b,
0x007b,0x007b,0x007c,0x007c,0x007c,0x007c,0x007c,0x007c,0x007c,0x007c,0x007d,0x007d,0x007d,
0x007d,0x007d,0x007d,0x007e,0x007e,0x007e,0x007e,0x007e,0x007e,0x007e,0x007f,0x007f,0x007f,
0x007f,0x007f,0x007f,0x007f,0x009f,0x009f,0x009f,0x009f,0x00bf,0x00bf,0x00bf,0x00be,0x00de,
0x00de,0x00de,0x00fe,0x00fe,0x00fd,0x00fd,0x011d,0x011d,0x011d,0x013d,0x013d,0x013d,0x013c,
0x015c,0x015c,0x015c,0x015c,0x017c,0x017c,0x017c,0x017b,0x019b,0x019b,0x019b,0x01bb,0x01bb,
0x01bb,0x01ba,0x01da,0x01da,0x01da,0x01fa,0x01fa,0x01fa,0x01f9,0x0219,0x0219,0x0219,0x0219,
0x0239,0x0239,0x0239,0x0238,0x0258,0x0258,0x0258,0x0258,0x0278,0x0278,0x0277,0x0297,0x0297,
0x0297,0x02b7,0x02b7,0x02b7,0x02b6,0x02d6,0x02d6,0x02d6,0x02d6,0x02f6,0x02f6,0x02f6,0x02f5,
0x0315,0x0315,0x0315,0x0315,0x0335,0x0335,0x0334,0x0354,0x0354,0x0354,0x0374,0x0374,0x0374,
0x0373,0x0393,0x0393,0x0393,0x0393,0x03b3,0x03b3,0x03b3,0x03b2,0x03d2,0x03d2,0x03d2,0x03d2,
0x03f2,0x03f2,0x03f1,0x0411,0x0411,0x0411,0x0411,0x0431,0x0431,0x0430,0x0450,0x0450,0x0450,
0x0450,0x0470,0x0470,0x0470,0x046f,0x048f,0x048f,0x048f,0x048f,0x04af,0x04af,0x04ae,0x04ce,
0x04ce,0x04ce,0x04ce,0x04ee,0x04ee,0x04ed,0x050d,0x050d,0x050d,0x050d,0x052d,0x052d,0x052d,
0x052c,0x054c,0x054c,0x054c,0x054c,0x056c,0x056c,0x056b,0x058b,0x058b,0x058b,0x058b,0x05ab,
0x05ab,0x05aa,0x05ca,0x05ca,0x05ca,0x05ca,0x05ea,0x05ea,0x05ea,0x05e9,0x0609,0x0609,0x0609,
0x0609,0x0629,0x0629,0x0628,0x0648,0x0648,0x0648,0x0648,0x0668,0x0668,0x0667,0x0687,0x0687,
0x0687,0x0687,0x06a7,0x06a7,0x06a7,0x06a6,0x06c6,0x06c6,0x06c6,0x06c6,0x06e6,0x06e6,0x06e5,
0x06e5,0x0705,0x0705,0x0705,0x0725,0x0725,0x0724,0x0744,0x0744,0x0744,0x0744,0x0764,0x0764,
0x0764,0x0763,0x0783,0x0783,0x0783,0x0783,0x07a3,0x07a3,0x07a2,0x07a2,0x07c2,0x07c2,0x07c2,
0x07e2,0x07e2,0x07e1,0x07e1,0x07e1,0x07e1,0x07e1,0x0fe1,0x07e1,0x0fe1,0x0fe1,0x0fe1,0x0fe1,
0x0fe1,0x0fe1,0x0fe1,0x0fe1,0x17e1,0x17e1,0x17e1,0x17e1,0x17e1,0x1fe1,0x17e1,0x1fe1,0x1fe1,
0x1fe1,0x1fe1,0x1fe1,0x27e1,0x27e1,0x27e1,0x27e1,0x27e1,0x27e1,0x27e1,0x27e1,0x2fe1,0x27e1,
0x2fe1,0x2fe1,0x2fe1,0x2fe1,0x2fe1,0x2fe1,0x2fe1,0x2fe1,0x37e1,0x37e1,0x37e1,0x37c1,0x37e1,
0x3fc1,0x37e1,0x3fc1,0x3fe1,0x3fc1,0x3fc1,0x47c1,0x47c1,0x47c1,0x47c1,0x47c1,0x47c1,0x47c1,
0x4fc1,0x47c1,0x4fc1,0x47c1,0x4fc1,0x4fc1,0x4fc1,0x4fc1,0x4fc1,0x4fc1,0x4fc1,0x4fc1,0x57c1,
0x57c1,0x57c1,0x5fc1,0x57c1,0x5fc1,0x5fc1,0x5fc1,0x5fc1,0x5fc1,0x67c1,0x67c1,0x67c1,0x67c1,
0x67c1,0x67c1,0x67c1,0x67c1,0x6fc1,0x67c1,0x6fc1,0x67c1,0x6fc1,0x6fc1,0x6fc1,0x6fc1,0x6fc1,
0x6fc1,0x6fc1,0x77c1,0x77c1,0x77a0,0x77c1,0x7fa0,0x77c1,0x7fa0,0x7fc1,0x7fa0,0x7fa0,0x7fa0,
0x87a0,0x87a0,0x87a0,0x87a0,0x8fa2,0x8fa2,0x8fa2,0x87a0,0x8fa2,0x87a0,0x8fa2,0x8fa0,0x8fa0,
0x8fa0,0x8fa0,0x8fa0,0x8fa0,0x97a2,0x97a0,0x9fa2,0x97a0,0x9fa2,0x97a0,0x9fa2,0x9fa2,0x9fa2,
0xa7a2,0xa7a2,0xa7a2,0x9fa0,0xa7a2,0x9fa0,0xa7a2,0xa7a0,0xa7a0,0xa7a0,0xa7a0,0xa7a0,0xa7a0,
0xa7a0,0xafa0,0xb7a2,0xafa0,0xb7a2,0xafa0,0xb7a2,0xb7a2,0xb7a2,0xbfa2,0xbfa2,0xbfa2,0xb780,
0xbfa2,0xb780,0xbfa2,0xbf80,0xc7a2,0xbf80,0xbf80,0xbf80,0xbf80,0xbf80,0xc780,0xcf82,0xc780,
0xcf82,0xc780,0xcf82,0xcf82,0xcf82,0xd782,0xd782,0xd782,0xd782,0xd782,0xcf80,0xd782,0xd780,
0xdf82,0xd780,0xd780,0xd780,0xd780,0xd780,0xdf80,0xe782,0xdf80,0xe782,0xdf80,0xe782,0xdf80,
0xe782,0xef82,0xef82,0xef82,0xef82,0xef82,0xe780,0xef82,0xef80,0xf782,0xef80,0xef80,0xef80,
0xef80,0xef80,0xf780,0xf760,0xf780,0xf760,0xf760,0xf760,0xf762,0xf760,0xf762,0xf760,0xf762,
0xf760,0xf762,0xf740,0xf742,0xf742,0xf742,0xf742,0xf742,0xf742,0xf742,0xf742,0xf740,0xf742,
0xf740,0xf722,0xf721,0xf723,0xf721,0xf721,0xf721,0xf721,0xf721,0xf721,0xf721,0xf721,0xf723,
0xf701,0xf703,0xf701,0xf703,0xf701,0xf703,0xf703,0xf703,0xf703,0xf703,0xf703,0xf703,0xf6e3,
0xf6e1,0xf6e3,0xf6e2,0xf6e4,0xf6e2,0xf6e4,0xf6e2,0xf6e4,0xf6e2,0xf6e2,0xf6e2,0xf6c2,0xf6c2,
0xf6c2,0xf6c2,0xf6c2,0xf6c4,0xf6c2,0xf6c4,0xf6c2,0xf6c4,0xf6c2,0xf6c4,0xf6a4,0xf6a4,0xf6a4,
0xf6a4,0xf6a4,0xf6a5,0xf6a5,0xf6a3,0xf6a5,0xf6a3,0xf685,0xf6a3,0xf685,0xf683,0xf683,0xf683,
0xf683,0xf683,0xf683,0xf683,0xf683,0xf685,0xf663,0xf685,0xf663,0xf665,0xf663,0xf665,0xf663,
0xf665,0xf665,0xf666,0xf666,0xf666,0xf646,0xf646,0xf646,0xfe44,0xf646,0xfe44,0xf646,0xfe44,
0xf646,0xfe44,0xfe44,0xfe44,0xfe24,0xfe24,0xfe24,0xfe24,0xfe24,0xfe26,0xfe24,0xfe26,0xfe24,
0xfe27,0xfe24,0xfe27,0xfe07,0xfe07,0xfe07,0xfe07,0xfe07,0xfe07,0xfe07,0xfe07,0xfe07,0xfe05,
0xfe07,0xfe05,0xfde7,0xfde5,0xfde7,0xfde5,0xfde5,0xfde5,0xfde5,0xfde5,0xfde5,0xfde5,0xfde5,
0xfde8,0xfdc5,0xfdc8,0xfdc6,0xfdc8,0xfdc6,0xfdc8,0xfdc8,0xfdc8,0xfdc8,0xfdc8,0xfdc8,0xfdc8,
0xfda8,0xfda6,0xfda8,0xfda6,0xfda8,0xfda6,0xfda8,0xfda6,0xfda6,0xfda6,0xfda6,0xfda6,0xfd86,
0xfd87,0xfd86,0xfd87,0xfd87,0xfd89,0xfd87,0xfd89,0xfd87,0xfd89,0xfd67,0xfd89,0xfd69,0xfd69,
0xfd69,0xfd69,0xfd69,0xfd69,0xfd69,0xfd67,0xfd69,0xfd67,0xfd49,0xfd67,0xfd49,0xfd47,0xfd47,
0xfd48,0xfd47,0xfd48,0xfd48,0xfd48,0xfd48,0xfd4a,0xfd28,0xfd2a,0xfd28,0xfd2a,0xfd28,0xfd2a,
0xfd2a,0xfd2a,0xfd2a,0xfd2a,0xfd0a,0xfd0a,0xfd0a,0xfd0a,0xfd0a,0xfd0a,0xfcea,0xfcea,0xfcea,
0xfcea,0xfcea,0xfcca,0xfcc9,0xfcc9,0xfcc9,0xfcc9,0xfca9,0xfca9,0xfca9,0xfca9,0xfca9,0xfca9,
0xfc89,0xfc89,0xfc89,0xfc89,0xfc89,0xfc69,0xfc69,0xfc69,0xfc69,0xfc69,0xfc49,0xfc49,0xfc49,
0xfc49,0xfc49,0xfc49,0xfc28,0xfc28,0xfc28,0xfc28,0xfc28,0xfc08,0xfc08,0xfc08,0xfc08,0xfc08,
0xfbe8,0xfbe8,0xfbe8,0xfbe8,0xfbe8,0xfbe8,0xfbc8,0xfbc8,0xfbc8,0xfbc8,0xfbc8,0xfba8,0xfba8,
0xfba8,0xfba8,0xfba8,0xfb87,0xfb87,0xfb87,0xfb87,0xfb87,0xfb87,0xfb67,0xfb67,0xfb67,0xfb67,
0xfb67,0xfb47,0xfb47,0xfb47,0xfb47,0xfb47,0xfb27,0xfb27,0xfb27,0xfb27,0xfb27,0xfb27,0xfb07,
0xfb07,0xfb07,0xfb07,0xfb06,0xfae6,0xfae6,0xfae6,0xfae6,0xfae6,0xfac6,0xfac6,0xfac6,0xfac6,
0xfac6,0xfac6,0xfaa6,0xfaa6,0xfaa6,0xfaa6,0xfaa6,0xfa86,0xfa86,0xfa86,0xfa86,0xfa86,0xfa66,
0xfa66,0xfa66,0xfa66,0xfa65,0xfa65,0xfa45,0xfa45,0xfa45,0xfa45,0xfa45,0xfa25,0xfa25,0xfa25,
0xfa25,0xfa25,0xfa05,0xfa05,0xfa05,0xfa05,0xfa05,0xfa05,0xf9e5,0xf9e5,0xf9e5,0xf9e5,0xf9e5,
0xf9e5,0xf9c5,0xf9c5,0xf9c4,0xf9c4,0xf9a4,0xf9a4,0xf9a4,0xf9a4,0xf9a4,0xf9a4,0xf984,0xf984,
0xf984,0xf984,0xf984,0xf984,0xf964,0xf964,0xf964,0xf964,0xf944,0xf944,0xf944,0xf944,0xf944,
0xf944,0xf924,0xf924,0xf923,0xf923,0xf923,0xf923,0xf903,0xf903,0xf903,0xf903,0xf8e3,0xf8e3,
0xf8e3,0xf8e3,0xf8e3,0xf8e3,0xf8c3,0xf8c3,0xf8c3,0xf8c3,0xf8c3,0xf8c3,0xf8a3,0xf8a3,0xf8a3,
0xf8a3,0xf883,0xf883,0xf882,0xf882,0xf882,0xf882,0xf862,0xf862,0xf862,0xf862,0xf862,0xf862,
0xf842,0xf842,0xf842,0xf842,0xf822,0xf822,0xf822,0xf822,0xf822,0xf822,0xf802,0xf802,0xf802,
0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,
0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802};
#endif
#if colorMode == 21  //this is alternate color map 
/* from https://www.thingiverse.com/thing:3017556
1st
#000000
 2nd
#000fff
 3rd
#00ff0f
 4th
#ff0000
 5th
#FfA544
 6th
#F0F000
 */
const PROGMEM uint16_t camColors[] =
{0x002e,0x002e,0x002e,0x002f,0x002f,0x002f,0x002f,0x002f,0x002f,0x004f,0x002f,0x0050,0x0030,
0x0050,0x0030,0x0050,0x0050,0x0051,0x0051,0x0051,0x0051,0x0051,0x0051,0x0051,0x0052,0x0052,
0x0052,0x0052,0x0052,0x0052,0x0052,0x0053,0x0053,0x0053,0x0053,0x0053,0x0053,0x0053,0x0054,
0x0054,0x0054,0x0054,0x0054,0x0054,0x0054,0x0055,0x0055,0x0055,0x0055,0x0055,0x0055,0x0055,
0x0056,0x0056,0x0056,0x0056,0x0056,0x0056,0x0056,0x0057,0x0057,0x0057,0x0057,0x0057,0x0057,
0x0057,0x0058,0x0078,0x0058,0x0078,0x0058,0x0078,0x0058,0x0079,0x0059,0x0079,0x0079,0x0079,
0x0079,0x007a,0x007a,0x007a,0x007a,0x007a,0x007a,0x007a,0x007a,0x007b,0x007b,0x007b,0x007b,
0x007b,0x007b,0x007c,0x007c,0x007c,0x007c,0x007c,0x007c,0x007c,0x007c,0x007d,0x007d,0x007d,
0x007d,0x007d,0x007d,0x007e,0x007e,0x007e,0x007e,0x007e,0x007e,0x007e,0x007f,0x007f,0x007f,
0x007f,0x007f,0x007f,0x007f,0x009f,0x009f,0x009f,0x009f,0x00bf,0x00bf,0x00bf,0x00be,0x00de,
0x00de,0x00de,0x00fe,0x00fe,0x00fd,0x00fd,0x011d,0x011d,0x011d,0x013d,0x013d,0x013d,0x013c,
0x015c,0x015c,0x015c,0x015c,0x017c,0x017c,0x017c,0x017b,0x019b,0x019b,0x019b,0x01bb,0x01bb,
0x01bb,0x01ba,0x01da,0x01da,0x01da,0x01fa,0x01fa,0x01fa,0x01f9,0x0219,0x0219,0x0219,0x0219,
0x0239,0x0239,0x0239,0x0238,0x0258,0x0258,0x0258,0x0258,0x0278,0x0278,0x0277,0x0297,0x0297,
0x0297,0x02b7,0x02b7,0x02b7,0x02b6,0x02d6,0x02d6,0x02d6,0x02d6,0x02f6,0x02f6,0x02f6,0x02f5,
0x0315,0x0315,0x0315,0x0315,0x0335,0x0335,0x0334,0x0354,0x0354,0x0354,0x0374,0x0374,0x0374,
0x0373,0x0393,0x0393,0x0393,0x0393,0x03b3,0x03b3,0x03b3,0x03b2,0x03d2,0x03d2,0x03d2,0x03d2,
0x03f2,0x03f2,0x03f1,0x0411,0x0411,0x0411,0x0411,0x0431,0x0431,0x0430,0x0450,0x0450,0x0450,
0x0450,0x0470,0x0470,0x0470,0x046f,0x048f,0x048f,0x048f,0x048f,0x04af,0x04af,0x04ae,0x04ce,
0x04ce,0x04ce,0x04ce,0x04ee,0x04ee,0x04ed,0x050d,0x050d,0x050d,0x050d,0x052d,0x052d,0x052d,
0x052c,0x054c,0x054c,0x054c,0x054c,0x056c,0x056c,0x056b,0x058b,0x058b,0x058b,0x058b,0x05ab,
0x05ab,0x05aa,0x05ca,0x05ca,0x05ca,0x05ca,0x05ea,0x05ea,0x05ea,0x05e9,0x0609,0x0609,0x0609,
0x0609,0x0629,0x0629,0x0628,0x0648,0x0648,0x0648,0x0648,0x0668,0x0668,0x0667,0x0687,0x0687,
0x0687,0x0687,0x06a7,0x06a7,0x06a7,0x06a6,0x06c6,0x06c6,0x06c6,0x06c6,0x06e6,0x06e6,0x06e5,
0x06e5,0x0705,0x0705,0x0705,0x0725,0x0725,0x0724,0x0744,0x0744,0x0744,0x0744,0x0764,0x0764,
0x0764,0x0763,0x0783,0x0783,0x0783,0x0783,0x07a3,0x07a3,0x07a2,0x07a2,0x07c2,0x07c2,0x07c2,
0x07e2,0x07e2,0x07e1,0x07e1,0x07e1,0x07e1,0x07c1,0x07c1,0x07c1,0x07c1,0x0fa1,0x0fa1,0x0fa1,
0x1781,0x0f81,0x1781,0x1761,0x1761,0x1761,0x1761,0x1f41,0x1741,0x1741,0x1f41,0x1f21,0x1f21,
0x1f21,0x2701,0x2701,0x2701,0x26e1,0x26e1,0x26e1,0x26e1,0x26c1,0x26c1,0x26c1,0x2ea1,0x2ea1,
0x2ea1,0x2e81,0x3681,0x3681,0x2e81,0x3661,0x3661,0x3661,0x3661,0x3641,0x3641,0x3641,0x3e21,
0x3e21,0x3e21,0x3e01,0x4601,0x4601,0x3e01,0x45e1,0x45e1,0x45e1,0x45e1,0x45c1,0x4dc1,0x45c1,
0x4da1,0x4da1,0x4da1,0x4d81,0x4d81,0x5581,0x4d81,0x5561,0x5561,0x5561,0x5d41,0x5541,0x5d41,
0x5541,0x5d21,0x6523,0x6523,0x6503,0x6503,0x5d01,0x6503,0x64e1,0x64e1,0x64e1,0x64c1,0x6cc3,
0x64c1,0x6ca3,0x74a3,0x74a3,0x74a3,0x6c81,0x7483,0x6c81,0x7481,0x7461,0x7461,0x7461,0x7441,
0x7c43,0x7441,0x7c23,0x8423,0x8422,0x8423,0x7c00,0x8403,0x7c00,0x8401,0x83e0,0x83e0,0x8be2,
0x83c0,0x8bc2,0x8bc2,0x8ba2,0x93a2,0x8ba0,0x93a2,0x8b80,0x8b80,0x8b80,0x9360,0x9b62,0x9360,
0x9b42,0x9340,0x9b42,0x9b42,0x9b22,0xa322,0x9b20,0xa322,0x9b00,0x9b00,0x9b00,0xa2e0,0xaae2,
0xa2e0,0xaac2,0xaac2,0xaac2,0xaac2,0xaaa0,0xb2a2,0xaaa0,0xaaa0,0xaa80,0xaa80,0xb282,0xb260,
0xba62,0xba62,0xba42,0xba42,0xb240,0xba42,0xba20,0xc222,0xba20,0xba00,0xba00,0xba00,0xc1e2,
0xc1e0,0xc9e2,0xc9e2,0xc9c2,0xc9c2,0xc1c0,0xc9c2,0xc9a0,0xc9a0,0xc9a0,0xc980,0xd182,0xc980,
0xd162,0xd962,0xd962,0xd962,0xd140,0xd942,0xd140,0xd940,0xd920,0xd920,0xe122,0xd900,0xe102,
0xd900,0xe0e2,0xe8e2,0xe8e2,0xe8e2,0xe0c0,0xe8c2,0xe0c0,0xe8c0,0xe8a0,0xe8a0,0xf0a2,0xe880,
0xf082,0xf082,0xf062,0xf062,0xf060,0xf062,0xf040,0xf040,0xf040,0xf820,0xf822,0xf820,0xf802,
0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf822,0xf822,0xf822,0xf822,0xf822,
0xf842,0xf842,0xf842,0xf842,0xf842,0xf862,0xf862,0xf862,0xf862,0xf862,0xf862,0xf882,0xf882,
0xf882,0xf882,0xf883,0xf8a3,0xf8a3,0xf8a3,0xf8a3,0xf8a3,0xf8c3,0xf8c3,0xf8c3,0xf8c3,0xf8c3,
0xf8c3,0xf8e3,0xf8e3,0xf8e3,0xf8e3,0xf8e3,0xf903,0xf903,0xf903,0xf903,0xf903,0xf923,0xf923,
0xf923,0xf923,0xf924,0xf924,0xf944,0xf944,0xf944,0xf944,0xf944,0xf964,0xf964,0xf964,0xf964,
0xf964,0xf984,0xf984,0xf984,0xf984,0xf984,0xf984,0xf9a4,0xf9a4,0xf9a4,0xf9a4,0xf9a4,0xf9c4,
0xf9c4,0xf9c4,0xf9c5,0xf9c5,0xf9e5,0xf9e5,0xf9e5,0xf9e5,0xf9e5,0xf9e5,0xfa05,0xfa05,0xfa05,
0xfa05,0xfa05,0xfa25,0xfa25,0xfa25,0xfa25,0xfa25,0xfa45,0xfa45,0xfa45,0xfa45,0xfa45,0xfa45,
0xfa65,0xfa65,0xfa66,0xfa66,0xfa66,0xfa66,0xfa86,0xfa86,0xfa86,0xfa86,0xfaa6,0xfaa6,0xfaa6,
0xfaa6,0xfaa6,0xfaa6,0xfac6,0xfac6,0xfac6,0xfac6,0xfac6,0xfac6,0xfae6,0xfae6,0xfae6,0xfae6,
0xfb07,0xfb06,0xfb07,0xfb07,0xfb07,0xfb07,0xfb27,0xfb27,0xfb27,0xfb27,0xfb27,0xfb27,0xfb47,
0xfb47,0xfb47,0xfb47,0xfb67,0xfb67,0xfb67,0xfb67,0xfb67,0xfb67,0xfb87,0xfb87,0xfb87,0xfb87,
0xfb88,0xfb87,0xfba8,0xfba8,0xfba8,0xfba8,0xfbc8,0xfbc8,0xfbc8,0xfbc8,0xfbc8,0xfbc8,0xfbe8,
0xfbe8,0xfbe8,0xfbe8,0xfbe8,0xfbe8,0xfc08,0xfc08,0xfc08,0xfc08,0xfc28,0xfc28,0xfc28,0xfc28,
0xfc29,0xfc28,0xfc49,0xfc49,0xfc49,0xfc49,0xfc49,0xfc49,0xfc69,0xfc69,0xfc69,0xfc69,0xfc89,
0xfc89,0xfc89,0xfc89,0xfc89,0xfc89,0xfca9,0xfca9,0xfca9,0xfca9,0xfca9,0xfca9,0xfcc9,0xfcc9,
0xfcca,0xfcc9,0xfcea,0xfcea,0xfcea,0xfcea,0xfcea,0xfcea,0xfd0a,0xfd0a,0xfd0a,0xfd0a,0xfd0a,
0xfd0a,0xfd2a,0xfd2a,0xfd2a,0xfd28,0xfd2a,0xfd28,0xfd2a,0xfd28,0xfd4a,0xfd28,0xfd4a,0xfd48,
0xfd48,0xfd48,0xfd48,0xfd47,0xfd47,0xfd47,0xfd47,0xfd49,0xfd67,0xfd49,0xfd67,0xfd69,0xfd67,
0xfd69,0xfd69,0xfd69,0xfd69,0xfd69,0xfd69,0xfd69,0xfd89,0xfd87,0xfd89,0xfd87,0xfd89,0xfd87,
0xfd89,0xfd87,0xfd87,0xfd86,0xfd86,0xfd86,0xfda6,0xfda6,0xfda6,0xfda8,0xfda6,0xfda8,0xfda6,
0xfda8,0xfda6,0xfda8,0xfda6,0xfda8,0xfdc8,0xfdc8,0xfdc8,0xfdc8,0xfdc8,0xfdc8,0xfdc8,0xfdc6,
0xfdc8,0xfdc6,0xfdc8,0xfdc5,0xfde7,0xfde5,0xfde5,0xfde5,0xfde5,0xfde5,0xfde5,0xfde5,0xfde5,
0xfde7,0xfde5,0xfde7,0xfe05,0xfe07,0xfe05,0xfe07,0xfe07,0xfe07,0xfe07,0xfe07,0xfe07,0xfe07,
0xfe07,0xfe05,0xfe27,0xfe24,0xfe26,0xfe24,0xfe26,0xfe24,0xfe26,0xfe24,0xfe24,0xfe24,0xfe44,
0xfe24,0xfe44,0xfe44,0xfe44,0xf646,0xfe44,0xf646,0xfe44,0xf646,0xfe44,0xf646,0xf666,0xf646,
0xf666,0xf666,0xf666,0xf665,0xf665,0xf663,0xf665,0xf663,0xf665,0xf663,0xf685,0xf683,0xf683,
0xf683,0xf683,0xf683,0xf683,0xf683,0xf683,0xf685,0xf683,0xf685,0xf6a3,0xf6a5,0xf6a3,0xf6a5,
0xf6a3,0xf6a5,0xf6a5,0xf6a4,0xf6a4,0xf6a4,0xf6a4,0xf6a4,0xf6c4,0xf6c2,0xf6c4,0xf6c2,0xf6c4,
0xf6c2,0xf6c4,0xf6c2,0xf6c2,0xf6c2,0xf6c2,0xf6c2,0xf6e2,0xf6e2,0xf6e2,0xf6e4,0xf6e2,0xf6e4,
0xf6e2,0xf6e4,0xf6e2,0xf6e3,0xf6e3,0xf6e3,0xf703,0xf703,0xf703,0xf703,0xf703,0xf703,0xf703,
0xf701,0xf703,0xf701,0xf703,0xf701,0xf723,0xf721,0xf721,0xf721,0xf721,0xf721,0xf721,0xf721,
0xf721,0xf723,0xf721,0xf722,0xf740,0xf742,0xf740,0xf742,0xf742,0xf742,0xf742,0xf742,0xf742,
0xf742,0xf762,0xf740,0xf762,0xf760,0xf762,0xf760,0xf762,0xf760,0xf760,0xf760,0xf760,0xf760,
0xf780,0xf760,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780};
#endif

#if colorMode == 22  //this is alternate color map with shades of grey except for hot areas
/* from https://www.thingiverse.com/thing:3017556
1st
#000000
 2nd
#AAAAAA
 3rd
#CCCCCC
 4th
#FFFFFF
 5th
#FfA544
 6th
#FF0000
 */
const PROGMEM uint16_t camColors[] = 
{0x4a69,0x4a69,0x4a69,0x528a,0x528a,0x528a,0x528a,0x528a,0x528a,0x52aa,0x52aa,0x52aa,0x52aa,0x5acb,0x5acb,0x5acb,0x5acb,
0x5acb,0x5acb,0x5aeb,0x5aeb,0x5aeb,0x5aeb,0x5aeb,0x630c,0x630c,0x630c,0x630c,0x630c,0x632c,0x632c,0x632c,0x632c,0x632c,
0x632c,0x6b4d,0x6b4d,0x6b4d,0x6b4d,0x6b6d,0x6b6d,0x6b6d,0x6b6d,0x6b6d,0x6b6d,0x738e,0x738e,0x738e,0x738e,0x738e,0x73ae,
0x73ae,0x73ae,0x73ae,0x73ae,0x7bcf,0x7bcf,0x7bcf,0x7bcf,0x7bcf,0x7bcf,0x7bef,0x7bef,0x7bef,0x7bef,0x8410,0x8410,0x8410,
0x8410,0x8410,0x8410,0x8430,0x8430,0x8430,0x8430,0x8430,0x8c51,0x8c51,0x8c51,0x8c51,0x8c51,0x8c71,0x8c71,0x8c71,0x8c71,
0x8c71,0x8c71,0x9492,0x9492,0x9492,0x9492,0x94b2,0x94b2,0x94b2,0x94b2,0x94b2,0x94b2,0x9cd3,0x9cd3,0x9cd3,0x9cd3,0x9cd3,
0x9cf3,0x9cf3,0x9cf3,0x9cf3,0x9cf3,0xa514,0xa514,0xa514,0xa514,0xa514,0xa514,0xa534,0xa534,0xa534,0xa534,0xad55,0xad55,
0xad55,0xad55,0xad55,0xad55,0xad55,0xad55,0xad55,0xad55,0xad55,0xad55,0xad75,0xad55,0xad75,0xad55,0xad75,0xad75,0xad75,
0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,0xad75,
0xad75,0xad75,0xb596,0xad75,0xb596,0xad75,0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,
0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,0xb596,0xb5b6,0xb596,0xb5b6,0xb596,0xb5b6,0xb5b6,
0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,0xb5b6,
0xb5b6,0xb5b6,0xb5b6,0xbdd7,0xb5b6,0xbdd7,0xb5b6,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,
0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdd7,0xbdf7,0xbdd7,0xbdf7,0xbdd7,0xbdf7,
0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xbdf7,
0xbdf7,0xbdf7,0xbdf7,0xbdf7,0xc618,0xbdf7,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,
0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc618,0xc638,0xc618,0xc638,0xc638,
0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,0xc638,
0xc638,0xc638,0xc638,0xc638,0xc638,0xce59,0xc638,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,
0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce59,0xce79,0xce59,0xce79,
0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,0xce79,
0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd69a,0xd6ba,
0xd69a,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,0xd6ba,
0xdedb,0xd6ba,0xdedb,0xdedb,0xdedb,0xdedb,0xdedb,0xdedb,0xdedb,0xdedb,0xdedb,0xdedb,0xdedb,0xdedb,0xdedb,0xdedb,0xdedb,
0xdedb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,0xdefb,
0xe71c,0xdefb,0xe71c,0xe71c,0xe71c,0xe71c,0xe71c,0xe71c,0xe71c,0xe71c,0xe71c,0xe71c,0xe71c,0xe71c,0xe71c,0xe71c,0xe71c,
0xe71c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,0xe73c,
0xe73c,0xe73c,0xef5d,0xef5d,0xef5d,0xef5d,0xef5d,0xef5d,0xef5d,0xef5d,0xef5d,0xef5d,0xef5d,0xef5d,0xef5d,0xef5d,0xef5d,
0xef5d,0xef7d,0xef5d,0xef7d,0xef7d,0xef7d,0xef7d,0xef7d,0xef7d,0xef7d,0xef7d,0xef7d,0xef7d,0xef7d,0xef7d,0xef7d,0xef7d,
0xef7d,0xef7d,0xf79e,0xf79e,0xf79e,0xf79e,0xf79e,0xf79e,0xf79e,0xf79e,0xf79e,0xf79e,0xf79e,0xf79e,0xf79e,0xf79e,0xf79e,
0xf79e,0xf7be,0xf79e,0xf7be,0xf7be,0xf7be,0xf7be,0xf7be,0xf7be,0xf7be,0xf7be,0xf7be,0xf7be,0xf7be,0xf7be,0xf7be,0xf7be,
0xf7be,0xf7be,0xffdf,0xf7be,0xffdf,0xffdf,0xffdf,0xffdf,0xffdf,0xffdf,0xffdf,0xffdf,0xffdf,0xffdf,0xffdf,0xffdf,0xffdf,
0xffdf,0xffdf,0xffdf,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,
0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffde,0xffde,0xffde,0xffde,0xffde,0xffde,0xffde,0xffde,
0xffde,0xffde,0xffbf,0xffbf,0xffbf,0xffbf,0xffbf,0xffbf,0xffbf,0xffbf,0xffbf,0xffbf,0xff9e,0xff9e,0xff9e,0xff9e,0xff9e,
0xff9e,0xff9e,0xff9e,0xff9d,0xff9d,0xff7d,0xff7d,0xff7d,0xff7d,0xff7d,0xff7d,0xff7d,0xff7d,0xff7c,0xff7c,0xff5c,0xff5c,
0xff5c,0xff5c,0xff5c,0xff5c,0xff5c,0xff5c,0xff3b,0xff5b,0xff3b,0xff3b,0xff3b,0xff3b,0xff3b,0xff3b,0xff3b,0xff3a,0xff1a,
0xff1a,0xff1a,0xff1a,0xff1a,0xff1a,0xff1a,0xff1a,0xff19,0xff19,0xfef9,0xfef9,0xfef9,0xfef9,0xfef9,0xfef9,0xfef9,0xfef9,
0xfef8,0xfef8,0xfed8,0xfed8,0xfed8,0xfed8,0xfed8,0xfed8,0xfed8,0xfed8,0xfed7,0xfed7,0xfeb7,0xfeb7,0xfeb7,0xfeb7,0xfeb7,
0xfeb7,0xfeb7,0xfeb6,0xfeb6,0xfeb6,0xfe96,0xfe96,0xfe96,0xfe96,0xfe96,0xfe96,0xfe95,0xfe95,0xfe95,0xfe95,0xfe75,0xfe75,
0xfe75,0xfe75,0xfe75,0xfe75,0xfe74,0xfe74,0xfe74,0xfe74,0xfe54,0xfe54,0xfe54,0xfe54,0xfe54,0xfe53,0xfe53,0xfe53,0xfe53,
0xfe53,0xfe33,0xfe33,0xfe33,0xfe33,0xfe32,0xfe32,0xfe32,0xfe32,0xfe32,0xfe32,0xfe12,0xfe12,0xfe12,0xfe12,0xfe11,0xfe11,
0xfe11,0xfe11,0xfdf1,0xfe11,0xfdf1,0xfdf1,0xfdf1,0xfdf1,0xfdf0,0xfdf0,0xfdf0,0xfdf0,0xfdd0,0xfdd0,0xfdd0,0xfdd0,0xfdd0,
0xfdcf,0xfdcf,0xfdcf,0xfdcf,0xfdcf,0xfdaf,0xfdaf,0xfdaf,0xfdaf,0xfdae,0xfdae,0xfdae,0xfdae,0xfdae,0xfdae,0xfd8e,0xfd8e,
0xfd8e,0xfd8e,0xfd8d,0xfd8d,0xfd8d,0xfd8d,0xfd8d,0xfd8d,0xfd6d,0xfd6d,0xfd6d,0xfd6c,0xfd6c,0xfd6c,0xfd6c,0xfd6c,0xfd6c,
0xfd6c,0xfd4c,0xfd4c,0xfd4b,0xfd4b,0xfd4b,0xfd4b,0xfd4b,0xfd4b,0xfd4b,0xfd4b,0xfd2b,0xfd2b,0xfd2a,0xfd2a,0xfd2a,0xfd2a,
0xfd2a,0xfd2a,0xfd0a,0xfd0a,0xfd0a,0xfd0a,0xfd0a,0xfd0a,0xfcea,0xfcea,0xfcea,0xfcea,0xfcea,0xfcca,0xfcc9,0xfcc9,0xfcc9,
0xfcc9,0xfca9,0xfca9,0xfca9,0xfca9,0xfca9,0xfca9,0xfc89,0xfc89,0xfc89,0xfc89,0xfc89,0xfc69,0xfc69,0xfc69,0xfc69,0xfc69,
0xfc49,0xfc49,0xfc49,0xfc49,0xfc49,0xfc49,0xfc28,0xfc28,0xfc28,0xfc28,0xfc28,0xfc08,0xfc08,0xfc08,0xfc08,0xfc08,0xfbe8,
0xfbe8,0xfbe8,0xfbe8,0xfbe8,0xfbe8,0xfbc8,0xfbc8,0xfbc8,0xfbc8,0xfbc8,0xfba8,0xfba8,0xfba8,0xfba8,0xfba8,0xfb87,0xfb87,
0xfb87,0xfb87,0xfb87,0xfb87,0xfb67,0xfb67,0xfb67,0xfb67,0xfb67,0xfb47,0xfb47,0xfb47,0xfb47,0xfb47,0xfb27,0xfb27,0xfb27,
0xfb27,0xfb27,0xfb27,0xfb07,0xfb07,0xfb07,0xfb07,0xfb06,0xfae6,0xfae6,0xfae6,0xfae6,0xfae6,0xfac6,0xfac6,0xfac6,0xfac6,
0xfac6,0xfac6,0xfaa6,0xfaa6,0xfaa6,0xfaa6,0xfaa6,0xfa86,0xfa86,0xfa86,0xfa86,0xfa86,0xfa66,0xfa66,0xfa66,0xfa66,0xfa65,
0xfa65,0xfa45,0xfa45,0xfa45,0xfa45,0xfa45,0xfa25,0xfa25,0xfa25,0xfa25,0xfa25,0xfa05,0xfa05,0xfa05,0xfa05,0xfa05,0xfa05,
0xf9e5,0xf9e5,0xf9e5,0xf9e5,0xf9e5,0xf9e5,0xf9c5,0xf9c5,0xf9c4,0xf9c4,0xf9a4,0xf9a4,0xf9a4,0xf9a4,0xf9a4,0xf9a4,0xf984,
0xf984,0xf984,0xf984,0xf984,0xf984,0xf964,0xf964,0xf964,0xf964,0xf944,0xf944,0xf944,0xf944,0xf944,0xf944,0xf924,0xf924,
0xf923,0xf923,0xf923,0xf923,0xf903,0xf903,0xf903,0xf903,0xf8e3,0xf8e3,0xf8e3,0xf8e3,0xf8e3,0xf8e3,0xf8c3,0xf8c3,0xf8c3,
0xf8c3,0xf8c3,0xf8c3,0xf8a3,0xf8a3,0xf8a3,0xf8a3,0xf883,0xf883,0xf882,0xf882,0xf882,0xf882,0xf862,0xf862,0xf862,0xf862,
0xf862,0xf862,0xf842,0xf842,0xf842,0xf842,0xf822,0xf822,0xf822,0xf822,0xf822,0xf822,0xf802,0xf802,0xf802,0xf802,0xf802,
0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,0xf802,
0xf802,0xf802,0xf802,0xf802};
#endif

#if colorMode == 23  //this is alternate color map with shades of grey except for hot areas
/* from https://www.thingiverse.com/thing:3017556
1st
#0000ff
 2nd
#00ff0f
 3rd
#F0F000
 4th
#F0F000
 5th
#660000
 6th
#ff0000
 */
const PROGMEM uint16_t camColors[] = 
{0x00fc,0x00fc,0x011c,0x011c,0x011b,0x011b,0x013b,0x013b,0x013b,0x015b,0x015b,0x015a,0x015a,0x017a,0x017a,0x017a,
0x019a,0x019a,0x019a,0x0199,0x01b9,0x01b9,0x01b9,0x01d9,0x01d9,0x01d9,0x01f8,0x01f8,0x01f8,0x01f8,0x0218,0x0218,
0x0218,0x0217,0x0237,0x0237,0x0237,0x0257,0x0257,0x0257,0x0277,0x0276,0x0276,0x0276,0x0296,0x0296,0x0296,0x0296,
0x02b5,0x02b5,0x02b5,0x02d5,0x02d5,0x02d5,0x02f5,0x02f4,0x02f4,0x02f4,0x0314,0x0314,0x0314,0x0334,0x0334,0x0333,
0x0353,0x0353,0x0353,0x0353,0x0373,0x0373,0x0372,0x0372,0x0392,0x0392,0x0392,0x03b2,0x03b2,0x03b1,0x03d1,0x03d1,
0x03d1,0x03d1,0x03f1,0x03f1,0x03f1,0x03f0,0x0410,0x0410,0x0410,0x0430,0x0430,0x0430,0x044f,0x044f,0x044f,0x044f,
0x046f,0x046f,0x046f,0x048e,0x048e,0x048e,0x04ae,0x04ae,0x04ae,0x04ae,0x04ce,0x04cd,0x04cd,0x04cd,0x04ed,0x04ed,
0x04ed,0x050d,0x050c,0x050c,0x052c,0x052c,0x052c,0x052c,0x054c,0x054b,0x054b,0x054b,0x056b,0x056b,0x056b,0x058b,
0x058b,0x058a,0x05aa,0x05aa,0x05aa,0x05aa,0x05ca,0x05ca,0x05c9,0x05c9,0x05e9,0x05e9,0x05e9,0x0609,0x0609,0x0608,
0x0628,0x0628,0x0628,0x0628,0x0648,0x0648,0x0648,0x0667,0x0667,0x0667,0x0687,0x0687,0x0687,0x0687,0x06a6,0x06a6,
0x06a6,0x06a6,0x06c6,0x06c6,0x06c6,0x06e5,0x06e5,0x06e5,0x0705,0x0705,0x0705,0x0705,0x0725,0x0724,0x0724,0x0724,
0x0744,0x0744,0x0744,0x0764,0x0764,0x0763,0x0783,0x0783,0x0783,0x0783,0x07a3,0x07a2,0x07a2,0x07c2,0x07c2,0x07c2,
0x07e2,0x07e2,0x07e2,0x07e1,0x07e1,0x07e1,0x07e1,0x07e1,0x0fe1,0x07e1,0x0fe1,0x0fe1,0x0fe1,0x0fe1,0x0fe1,0x0fe1,
0x0fe1,0x0fe1,0x17e1,0x17e1,0x17e1,0x17e1,0x17e1,0x1fe1,0x17e1,0x1fe1,0x1fe1,0x1fe1,0x1fe1,0x27e1,0x27e1,0x27e1,
0x27e1,0x27e1,0x27e1,0x27e1,0x2fe1,0x27e1,0x2fe1,0x27e1,0x2fe1,0x2fe1,0x2fe1,0x2fe1,0x2fe1,0x2fe1,0x2fe1,0x2fe1,
0x37e1,0x37e1,0x37e1,0x3fc1,0x37e1,0x3fc1,0x3fe1,0x3fc1,0x3fe1,0x3fc1,0x47c1,0x47c1,0x47c1,0x47c1,0x47c1,0x47c1,
0x47c1,0x47c1,0x4fc1,0x47c1,0x4fc1,0x47c1,0x4fc1,0x4fc1,0x4fc1,0x4fc1,0x4fc1,0x4fc1,0x4fc1,0x57c1,0x57c1,0x57c1,
0x57c1,0x5fc1,0x57c1,0x5fc1,0x5fc1,0x5fc1,0x5fc1,0x5fc1,0x67c1,0x67c1,0x67c1,0x67c1,0x67c1,0x67c1,0x67c1,0x67c1,
0x6fc1,0x67c1,0x6fc1,0x6fc1,0x6fc1,0x6fc1,0x6fc1,0x6fc1,0x6fc1,0x6fc1,0x77c1,0x77c1,0x77c1,0x77a0,0x77c1,0x7fa0,
0x77c1,0x7fa0,0x7fc1,0x7fa0,0x7fa0,0x87a0,0x87a0,0x87a0,0x87a0,0x87a0,0x87a0,0x87a0,0x87a0,0x87a0,0x87a0,0x87a0,
0x8fa0,0x97a2,0x8fa0,0x97a2,0x8fa0,0x97a2,0x97a2,0x97a2,0x9fa2,0x9fa2,0x9fa2,0x97a0,0x9fa2,0x97a0,0x9fa2,0x9fa0,
0xa7a2,0x9fa0,0x9fa0,0x9fa0,0x9fa0,0x9fa0,0xa7a0,0xafa2,0xa7a0,0xafa2,0xa7a0,0xafa2,0xafa2,0xafa2,0xb7a2,0xb7a2,
0xb7a2,0xb7a2,0xb7a2,0xafa0,0xb7a2,0xb7a0,0xbfa2,0xb780,0xb7a0,0xb780,0xb7a0,0xb780,0xbfa0,0xc782,0xbfa0,0xc782,
0xbf80,0xc782,0xbf80,0xc782,0xcf82,0xcf82,0xcf82,0xcf82,0xcf82,0xc780,0xcf82,0xcf80,0xd782,0xcf80,0xcf80,0xcf80,
0xcf80,0xcf80,0xd780,0xd780,0xd780,0xdf82,0xd780,0xdf82,0xd780,0xdf82,0xe782,0xe782,0xe782,0xe782,0xe782,0xdf80,
0xe782,0xe780,0xef82,0xe780,0xef82,0xe780,0xe780,0xe780,0xef80,0xef80,0xef80,0xf782,0xef80,0xf782,0xef80,0xf782,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,
0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf780,0xf762,0xf762,0xef60,
0xef60,0xf742,0xf742,0xf742,0xf742,0xef20,0xef20,0xf722,0xf702,0xef00,0xef00,0xe700,0xe6e0,0xeee2,0xeee2,0xe6c0,
0xe6c0,0xeec2,0xeec2,0xeea2,0xeea2,0xe6a0,0xe6a0,0xe682,0xe682,0xde80,0xde80,0xde60,0xde60,0xe662,0xe642,0xde40,
0xde40,0xe642,0xde20,0xde22,0xde22,0xd600,0xd600,0xde02,0xde02,0xd5e0,0xdde2,0xd5e0,0xd5e0,0xddc2,0xddc2,0xd5c0,
0xd5c0,0xd5a2,0xcda0,0xd5a2,0xd582,0xcd80,0xcd80,0xd582,0xd562,0xcd60,0xd562,0xcd40,0xcd40,0xcd42,0xcd42,0xc520,
0xc520,0xcd22,0xc520,0xcd02,0xcd02,0xc500,0xc500,0xcce2,0xcce2,0xc4e0,0xc4e2,0xbcc0,0xbcc0,0xc4c2,0xc4a2,0xbca0,
0xbca0,0xc482,0xbc80,0xc482,0xc482,0xbc60,0xbc60,0xbc62,0xbc62,0xb440,0xbc42,0xb440,0xb440,0xbc22,0xbc22,0xb420,
0xb420,0xbc02,0xb400,0xb402,0xb3e2,0xabe0,0xabe0,0xb3c2,0xb3c2,0xabc0,0xb3c2,0xaba0,0xaba0,0xb3a2,0xb3a2,0xab80,
0xab80,0xab82,0xa380,0xab62,0xab62,0xa360,0xa360,0xab42,0xab42,0xa340,0xab22,0xa320,0xa320,0xa322,0xa302,0x9b00,
0x9b00,0xa2e2,0x9ae0,0xa2e2,0xa2e2,0x9ac0,0x9ac0,0xa2c2,0xa2c2,0x9aa0,0x9aa2,0x92a0,0x92a0,0x9a82,0x9a82,0x9280,
0x9260,0x9a62,0x9260,0x9a62,0x9a42,0x9240,0x9240,0x9222,0x9222,0x8a20,0x9222,0x8a00,0x8a00,0x9202,0x9202,0x89e0,
0x89e0,0x91e2,0x89e0,0x89c2,0x89c2,0x81c0,0x81a0,0x89a2,0x89a2,0x81a0,0x8982,0x8180,0x8180,0x8962,0x8962,0x8160,
0x8160,0x8142,0x7940,0x8142,0x8142,0x7920,0x7920,0x8122,0x8122,0x7900,0x8102,0x7900,0x78e0,0x78e2,0x78e2,0x70e0,
0x70c0,0x78c2,0x70c0,0x78a2,0x78a2,0x70a0,0x70a0,0x7882,0x7882,0x7080,0x7082,0x6860,0x6860,0x7062,0x7062,0x6840,
0x6840,0x7042,0x6840,0x7022,0x7022,0x6820,0x6800,0x6802,0x6802,0x6000,0x6802,0x6802,0x6800,0x7002,0x6800,0x6800,
0x7002,0x7002,0x6800,0x7002,0x6800,0x6800,0x7002,0x7002,0x7802,0x7802,0x7000,0x7000,0x7802,0x7000,0x7802,0x7802,
0x7000,0x7000,0x7800,0x7800,0x8002,0x8002,0x7800,0x8002,0x7800,0x7800,0x8002,0x8002,0x8002,0x8002,0x8000,0x8000,
0x8802,0x8802,0x8802,0x8802,0x8000,0x8000,0x8802,0x8000,0x8802,0x8802,0x8800,0x8800,0x8800,0x8800,0x9002,0x9002,
0x8800,0x9002,0x8800,0x8800,0x9002,0x9002,0x9802,0x9802,0x9000,0x9000,0x9802,0x9000,0x9802,0x9802,0x9000,0x9000,
0x9800,0x9800,0xa002,0xa002,0x9800,0x9800,0x9800,0x9800,0xa002,0xa002,0x9800,0xa002,0xa000,0xa000,0xa802,0xa802,
0xa802,0xa802,0xa000,0xa000,0xa802,0xa000,0xa802,0xa802,0xa800,0xa800,0xa800,0xa800,0xb002,0xb002,0xa800,0xb002,
0xa800,0xa800,0xb002,0xb002,0xb000,0xb802,0xb000,0xb000,0xb802,0xb802,0xb802,0xb802,0xb000,0xb000,0xb802,0xb800,
0xc002,0xc002,0xb800,0xb800,0xb800,0xb800,0xc002,0xc002,0xb800,0xc002,0xc000,0xc000,0xc802,0xc802,0xc802,0xc802,
0xc000,0xc000,0xc802,0xc802,0xc802,0xc802,0xc800,0xc800,0xd002,0xc800,0xd002,0xd002,0xc800,0xc800,0xc800,0xc800,
0xd002,0xd002,0xd000,0xd802,0xd000,0xd000,0xd802,0xd802,0xd802,0xd802,0xd000,0xd000,0xd802,0xd800,0xe002,0xe002,};
#endif
# if colorMode == 3 //this is not implimented in code yet, as display currentlty used is only 12,16,18bit. this is 24 bit data
const PROGMEM uint32_t camColors[] =
{0x00021c,0x00011d,0x00021e,0x00021f,0x000220,0x000221,0x000223,0x000223,0x000225,0x000226,0x000326,0x000228,0x000328,
0x00022a,0x00032b,0x00022c,0x00032d,0x00032e,0x00032f,0x000330,0x000332,0x000332,0x000334,0x000335,0x000336,0x000337,
0x000438,0x000339,0x00043a,0x00033c,0x00043c,0x00033e,0x00043f,0x00043f,0x000441,0x000442,0x000443,0x000444,0x000446,
0x000446,0x000547,0x000449,0x000549,0x00044b,0x00054b,0x00044d,0x00054e,0x00054e,0x000550,0x000551,0x000552,0x000553,
0x000555,0x000555,0x000656,0x000558,0x000658,0x00055a,0x00065b,0x00055c,0x00065d,0x00065e,0x00065f,0x000660,0x000662,
0x000662,0x000664,0x000665,0x000666,0x000667,0x000768,0x000669,0x00076a,0x00066b,0x00076c,0x00066e,0x00076e,0x00076f,
0x000771,0x000771,0x000773,0x000774,0x000775,0x000776,0x000877,0x000778,0x000879,0x00077b,0x00087b,0x00077d,0x00087e,
0x00087e,0x000880,0x000881,0x000882,0x000883,0x000885,0x000885,0x000986,0x000888,0x000988,0x00088a,0x00098b,0x00088c,
0x00098d,0x00088e,0x00098f,0x000990,0x000991,0x000992,0x000994,0x000994,0x000996,0x000997,0x000a97,0x000999,0x000a9a,
0x00099b,0x000a9c,0x00099e,0x000a9e,0x000a9f,0x000aa1,0x000aa1,0x000aa3,0x000aa4,0x000aa5,0x000aa6,0x000ba7,0x000aa8,
0x000ba9,0x000aab,0x000bab,0x000aad,0x000bae,0x000bae,0x000bb0,0x000bb0,0x000bb2,0x000bb3,0x000bb4,0x000bb5,0x000cb6,
0x000bb7,0x000cb8,0x000bba,0x000cba,0x000bbc,0x000cbd,0x000bbe,0x000cbf,0x000cc0,0x000cc1,0x000cc2,0x000cc4,0x000cc4,
0x000cc6,0x000cc7,0x000dc7,0x000cc9,0x000dca,0x000ccb,0x000dcc,0x000cce,0x000dce,0x000dcf,0x000dd1,0x000dd1,0x000dd3,
0x000dd3,0x000dd5,0x000dd6,0x000ed6,0x000dd8,0x000ed9,0x000dda,0x000edb,0x000ddd,0x000edd,0x000ede,0x000ee0,0x000ee0,
0x000ee2,0x000ee3,0x000ee4,0x000ee5,0x000fe6,0x000ee7,0x000fe8,0x000eea,0x000fea,0x000eec,0x000fed,0x000eee,0x000fef,
0x000ff0,0x0010ee,0x0011ec,0x0012ea,0x0013e7,0x0014e6,0x0015e3,0x0017e1,0x0017df,0x0019dc,0x0019db,0x001bd8,0x001cd6,
0x001dd4,0x001ed1,0x001fd0,0x0020cd,0x0022cb,0x0022c9,0x0024c7,0x0024c5,0x0026c2,0x0026c1,0x0028be,0x0029bc,0x002aba,
0x002bb7,0x002cb6,0x002db3,0x002fb1,0x002faf,0x0031ac,0x0031ab,0x0033a8,0x0034a6,0x0035a4,0x0036a2,0x0037a0,0x00389d,
0x00399c,0x003a99,0x003c97,0x003c95,0x003e92,0x003e91,0x00408e,0x00418c,0x00428a,0x004387,0x004486,0x004583,0x004781,
0x00477f,0x00497d,0x00497b,0x004b78,0x004b77,0x004d74,0x004e72,0x004f70,0x00506d,0x00516c,0x005269,0x005467,0x005465,
0x005662,0x005661,0x00585e,0x00595c,0x005a5a,0x005b58,0x005c56,0x005d53,0x005e52,0x005f4f,0x00614d,0x00614b,0x006348,
0x006347,0x006544,0x006642,0x006740,0x00683d,0x00693c,0x006a39,0x006c37,0x006c35,0x006e33,0x006e31,0x00702e,0x00702d,
0x00722a,0x007328,0x007426,0x007523,0x007622,0x00771f,0x00791d,0x00791b,0x007b18,0x007b17,0x007d14,0x007e12,0x007f10,
0x00800e,0x00810c,0x008209,0x008308,0x008405,0x008603,0x008601,0x0087fe,0x0087fd,0x0089fa,0x008af8,0x008bf6,0x008cf3,
0x008df2,0x008eef,0x0090ed,0x0090eb,0x0092e9,0x0092e7,0x0094e4,0x0094e3,0x0096e0,0x0097de,0x0098dc,0x0099d9,0x009ad8,
0x009bd5,0x009dd3,0x009dd1,0x009fcf,0x009fcd,0x00a1ca,0x00a2c8,0x00a3c6,0x00a4c4,0x00a5c2,0x00a6bf,0x00a7be,0x00a8bb,
0x00aab9,0x00aab7,0x00acb4,0x00acb3,0x00aeb0,0x00afae,0x00b0ac,0x00b1aa,0x00b2a8,0x00b3a5,0x00b5a3,0x00b5a1,0x00b79f,
0x00b79d,0x00b99a,0x00b999,0x00bb96,0x00bc94,0x00bd92,0x00be8f,0x00bf8e,0x00c08b,0x00c289,0x00c287,0x00c485,0x00c483,
0x00c680,0x00c77e,0x00c87c,0x00c97a,0x00ca78,0x00cb75,0x00cc74,0x00cd71,0x00cf6f,0x00cf6d,0x00d16a,0x00d169,0x00d366,
0x00d464,0x00d562,0x00d660,0x00d75e,0x00d85b,0x00da59,0x00da57,0x00dc55,0x00dc53,0x00de50,0x00de4f,0x00e04c,0x00e14a,
0x00e248,0x00e345,0x00e444,0x00e541,0x00e73f,0x00e73d,0x00e93b,0x00e939,0x00eb36,0x00ec34,0x00ed32,0x00ee30,0x00ef2e,
0x00f02b,0x00f12a,0x00f227,0x00f425,0x00f423,0x00f620,0x00f61f,0x00f81c,0x00f91a,0x00fa18,0x00fb16,0x00fc14,0x00fd11,
0x00fe10,0x01fe10,0x03fe10,0x03fd10,0x05fe10,0x05fd10,0x07fe10,0x08fd10,0x09fe10,0x0afd10,0x0bfd10,0x0cfd10,0x0efd10,
0x0efd10,0x10fd10,0x10fd10,0x12fd10,0x12fc10,0x14fd10,0x15fc10,0x16fd10,0x17fc10,0x18fd10,0x19fc10,0x1bfc10,0x1bfc10,
0x1dfc10,0x1dfc10,0x1ffc10,0x20fc10,0x21fc10,0x22fb10,0x23fc10,0x24fb10,0x25fc10,0x26fb10,0x28fc10,0x28fb10,0x2afc10,
0x2afb10,0x2cfb10,0x2dfb10,0x2efb10,0x2ffb10,0x30fb10,0x31fb10,0x33fb10,0x33fa10,0x35fb10,0x35fa10,0x37fb10,0x37fa10,
0x39fb10,0x3afa10,0x3bfa10,0x3cfa10,0x3dfa10,0x3efa10,0x40fa10,0x40fa10,0x42fa10,0x42f910,0x44fa10,0x45f910,0x46fa10,
0x47f910,0x48fa10,0x49f910,0x4af910,0x4bf910,0x4df910,0x4df910,0x4ff910,0x4ff910,0x51f910,0x52f910,0x53f910,0x54f810,
0x55f910,0x56f810,0x58f910,0x58f810,0x5af910,0x5af810,0x5cf810,0x5cf810,0x5ef810,0x5ff810,0x60f810,0x61f810,0x62f810,
0x63f710,0x65f810,0x65f710,0x67f810,0x67f710,0x69f810,0x6af710,0x6bf710,0x6cf710,0x6df710,0x6ef710,0x6ff710,0x70f710,
0x72f710,0x72f610,0x74f710,0x74f610,0x76f710,0x77f610,0x78f710,0x79f610,0x7af610,0x7bf610,0x7df610,0x7df610,0x7ff610,
0x7ff610,0x81f610,0x81f610,0x83f610,0x84f510,0x85f610,0x86f510,0x87f610,0x88f510,0x8af610,0x8af510,0x8cf510,0x8cf510,
0x8ef510,0x8ff510,0x90f510,0x91f510,0x92f510,0x93f410,0x94f510,0x95f410,0x97f510,0x97f410,0x99f510,0x99f410,0x9bf410,
0x9cf410,0x9df410,0x9ef410,0x9ff410,0xa0f410,0xa2f410,0xa2f310,0xa4f410,0xa4f310,0xa6f410,0xa6f310,0xa8f410,0xa9f310,
0xaaf310,0xabf310,0xacf310,0xadf310,0xaff310,0xaff310,0xb1f310,0xb1f310,0xb3f310,0xb4f210,0xb5f310,0xb6f210,0xb7f310,
0xb8f210,0xb9f310,0xbaf210,0xbcf210,0xbcf210,0xbef210,0xbef210,0xc0f210,0xc1f210,0xc2f210,0xc3f110,0xc4f210,0xc5f110,
0xc7f210,0xc7f110,0xc9f210,0xc9f110,0xcbf110,0xcbf110,0xcdf110,0xcef110,0xcff110,0xd0f110,0xd1f110,0xd2f010,0xd4f110,
0xd4f010,0xd6f110,0xd6f010,0xd8f110,0xd9f010,0xdaf110,0xdbf010,0xdcf010,0xddf010,0xdef010,0xdff010,0xe1f010,0xe1f010,
0xe3f010,0xe3ef10,0xe5f010,0xe6ef10,0xe7f010,0xe8ef10,0xe9f010,0xeaef10,0xecef10,0xecef10,0xeeef10,0xeeef10,0xf0ef10,
0xf0ee11,0xf0ef11,0xf0ee11,0xf0ee12,0xf0ed13,0xf1ed14,0xf0ec15,0xf1ec16,0xf0ec15,0xf1ec16,0xf0eb17,0xf1eb18,0xf0ea19,
0xf1ea19,0xf1ea19,0xf1ea1a,0xf1e91b,0xf1e91c,0xf1e81c,0xf1e81d,0xf1e81d,0xf2e81e,0xf1e71f,0xf2e71f,0xf1e620,0xf2e621,
0xf1e621,0xf2e622,0xf2e522,0xf2e523,0xf2e424,0xf2e425,0xf2e326,0xf2e426,0xf2e326,0xf3e327,0xf2e228,0xf3e229,0xf2e12a,
0xf3e229,0xf2e12a,0xf3e12b,0xf3e02c,0xf3e02d,0xf3df2d,0xf3e02d,0xf3df2e,0xf3df2f,0xf3de30,0xf4de30,0xf3dd31,0xf4de31,
0xf3dd32,0xf4dd33,0xf3dc34,0xf4dc34,0xf3db35,0xf4db36,0xf4db36,0xf4db37,0xf4da37,0xf4da38,0xf4d939,0xf4d93a,0xf4d93a,
0xf5d93a,0xf4d83b,0xf5d83c,0xf4d73d,0xf5d73e,0xf4d73d,0xf5d73e,0xf5d63f,0xf5d640,0xf5d541,0xf5d541,0xf5d541,0xf5d542,
0xf5d443,0xf6d444,0xf5d345,0xf6d345,0xf5d246,0xf6d346,0xf5d247,0xf6d248,0xf6d148,0xf6d149,0xf6d04a,0xf6d14a,0xf6d04b,
0xf6d04b,0xf6cf4c,0xf7cf4d,0xf6ce4e,0xf7cf4e,0xf6ce4e,0xf7ce4f,0xf6cd50,0xf7cd51,0xf6cc52,0xf7cd51,0xf7cc52,0xf7cc53,
0xf7cb54,0xf7cb55,0xf7ca56,0xf7ca56,0xf7ca56,0xf8ca57,0xf7c958,0xf8c959,0xf7c859,0xf8c85a,0xf7c85a,0xf8c85b,0xf8c75c,
0xf8c75c,0xf8c65d,0xf8c65e,0xf8c65e,0xf8c65f,0xf8c55f,0xf9c560,0xf8c461,0xf9c462,0xf8c462,0xf9c463,0xf8c363,0xf9c364,
0xf9c265,0xf9c266,0xf9c167,0xf9c266,0xf9c167,0xf9c168,0xf9c069,0xf9c06a,0xf9bf6a,0xfac06a,0xf9bf6b,0xfabf6c,0xf9be6d,
0xfabe6d,0xf9bd6e,0xfabe6e,0xfabd6f,0xfabd70,0xfabc70,0xfabc71,0xfabb72,0xfabc72,0xfabb73,0xfbbb74,0xfaba74,0xfbba75,
0xfab976,0xfbb977,0xfab977,0xfbb977,0xfbb878,0xfbb879,0xfbb77a,0xfbb77b,0xfbb77a,0xfbb77b,0xfbb67c,0xfcb67d,0xfbb57e,
0xfcb57e,0xfbb57e,0xfcb57f,0xfbb480,0xfcb481,0xfcb382,0xfcb382,0xfcb382,0xfcb383,0xfcb284,0xfcb285,0xfcb185,0xfcb186,
0xfcb087,0xfdb187,0xfcb088,0xfdb088,0xfcaf89,0xfdaf8a,0xfcae8b,0xfdaf8b,0xfdae8b,0xfdae8c,0xfdad8d,0xfdad8e,0xfdac8f,
0xfdad8e,0xfdac8f,0xfeac90,0xfdab91,0xfeab92,0xfdaa93,0xfeab92,0xfdaa93,0xfeaa94,0xfea995,0xfea996,0xfea896,0xfea897,
0xfea897,0xfea898,0xfea799,0xffa799,0xfea69a,0xffa69b,0xfea69b,0xffa69c,0xfea59c,0xffa59d,0xffa49e,0xffa49f,0xffa3a0,
0xffa3a0,0xffa2a0,0xffa2a0,0xffa0a1,0xffa0a2,0xff9fa2,0xff9fa2,0xff9ea2,0xff9da3,0xff9ca3,0xff9ca4,0xff9ba4,0xff9aa5,
0xff99a5,0xff99a5,0xff98a6,0xff97a7,0xff96a7,0xff96a7,0xff95a7,0xff94a9,0xff93a9,0xff93a9,0xff92a9,0xff91aa,0xff90ab,
0xff90ab,0xff8fab,0xff8eac,0xff8dac,0xff8dad,0xff8cad,0xff8bae,0xff8aae,0xff8aae,0xff89ae,0xff88b0,0xff87b0,0xff87b0,
0xff86b0,0xff85b1,0xff84b2,0xff84b2,0xff83b2,0xff82b3,0xff81b3,0xff81b4,0xff80b4,0xff7fb5,0xff7eb5,0xff7eb5,0xff7db6,
0xff7cb7,0xff7bb7,0xff7bb7,0xff7ab7,0xff79b8,0xff78b9,0xff78b9,0xff77b9,0xff76ba,0xff75ba,0xff75bb,0xff74bb,0xff73bc,
0xff72bc,0xff72bc,0xff71bd,0xff70be,0xff6fbe,0xff6fbe,0xff6ebe,0xff6dc0,0xff6cc0,0xff6cc0,0xff6bc0,0xff6ac1,0xff69c2,
0xff69c2,0xff68c2,0xff67c3,0xff66c3,0xff66c3,0xff65c4,0xff64c5,0xff63c5,0xff63c5,0xff62c5,0xff61c7,0xff60c7,0xff60c7,
0xff5fc7,0xff5ec8,0xff5dc9,0xff5dc9,0xff5cc9,0xff5bca,0xff5aca,0xff5acb,0xff59cb,0xff58cc,0xff57cc,0xff57cc,0xff56cd,
0xff55ce,0xff54ce,0xff54ce,0xff53ce,0xff52cf,0xff51d0,0xff51d0,0xff50d0,0xff4fd1,0xff4ed1,0xff4ed2,0xff4dd2,0xff4cd3,
0xff4bd3,0xff4bd3,0xff4ad4,0xff49d5,0xff48d5,0xff48d5,0xff47d5,0xff46d7,0xff45d7,0xff45d7,0xff44d7,0xff43d8,0xff42d8,
0xff42d9,0xff41d9,0xff40da,0xff3fda,0xff3fda,0xff3edb,0xff3edb,0xff3cdc,0xff3cdc,0xff3bdc,0xff3bdd,0xff39de,0xff39de,
0xff38de,0xff38de,0xff36e0,0xff36e0,0xff35e0,0xff35e0,0xff33e1,0xff33e2,0xff32e2,0xff32e2,0xff30e3,0xff30e3,0xff2fe3,
0xff2fe4,0xff2de5,0xff2de5,0xff2ce5,0xff2ce5,0xff2ae7,0xff2ae7,0xff29e7,0xff29e7,0xff27e8};
#endif

//here we use a interupt timer. all we want to do is unload buffer while code is executing. in theory if we only do so often performance should increase!








Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

Adafruit_AMG88xx amg;
unsigned long delayTime;
int pixfx,pixfy=0;//used to smooth out and remove grid effect from errors in calc. a quick hack that may go away
float pixels[AMG88xx_PIXEL_ARRAY_SIZE];
#if interpolatemode == 1
uint16_t subpixelbuffer[16];//this is a buffer that holds pixel info to update several pixesl at a time!
#endif
#if spi_buffer_local == true //to write to buffer we create a command that adds to buffer and increments location
//this command buffer only works as long as pixel data is color is all that is being written, so no other lcd write commands before SpiFinAndFinalLCSCommandsForWrite()!
uint16_t spi_magic_local_buffer[256];//used for local spi. needs 16 bits wide for color, and each color needs to be processed, at least 2x2 for 64 pixel 
bool xSpihighbyte=false;//we keep track if high or low byte

uint8_t spiFrontbufrCount=0;//data is a ring
uint8_t spiBackbufrCount=0;// buffer.
//while we have this feature enabled we add in functions that will be used
//this command below does several things. if spi is busy it returns, rather than waits, if buffer is empty it just returns

void LowerSpiBuffer(){ if (!(SPSR & _BV(SPIF))){return;};
byte bufferbyte;//speeds things up
 //this one we have no choice but to wait for buffer to clear, so waiting is ok in this case, 
if (spiBackbufrCount != spiFrontbufrCount){xSpihighbyte = !xSpihighbyte;//we flip direction
if(xSpihighbyte){bufferbyte=highByte(spi_magic_local_buffer[spiBackbufrCount]);}//we flip before this instruction. so high byte goes first
else{bufferbyte=lowByte(spi_magic_local_buffer[spiBackbufrCount]);spiBackbufrCount++;};
#if spi_safety_checks != false //unless we have true we will have safety checks
while (!(SPSR & _BV(SPIF)));//we do check at begin side of loop so it most likely will pass 
#else //ok we have fast as possible
#endif
SPDR=bufferbyte;}


}
byte loadToSpiLocalBuffer(uint16_t x){spi_magic_local_buffer[spiFrontbufrCount]=x;spiFrontbufrCount++;}
//this is needed, if ready to write next pixel blocks of data and stuff still in buffer, we need to let it empty out first
void SpiFinAndFinalLcdCommandsForWrite(){
 //this one we have no choice but to wait for buffer to clear, so waiting is ok in this case, 
 byte bufferbyte;//speeds things up
while (spiBackbufrCount != spiFrontbufrCount){xSpihighbyte = !xSpihighbyte;//we flip direction
if(xSpihighbyte){bufferbyte=highByte(spi_magic_local_buffer[spiBackbufrCount]);}//we flip before this instruction. so high byte goes first
else{bufferbyte=lowByte(spi_magic_local_buffer[spiBackbufrCount]);spiBackbufrCount++;};
#if spi_safety_checks != false //unless we have true we will have safety checks
while (!(SPSR & _BV(SPIF)));//we do check at begin side of loop so it most likely will pass 
#else //ok we have fast as possible
//__asm__("nop\n\t"); __asm__("nop\n\t");__asm__("nop\n\t"); __asm__("nop\n\t");  __asm__("nop\n\t"); __asm__("nop\n\t"); 
//__asm__("nop\n\t"); __asm__("nop\n\t"); __asm__("nop\n\t"); __asm__("nop\n\t"); __asm__("nop\n\t"); __asm__("nop\n\t");__asm__("nop\n\t");// __asm__("nop\n\t");  

#endif
SPDR=bufferbyte;}
xSpihighbyte=false;tft.End64colorCommand();} //we reset byte order, you never know if a glitch. this fixes it within a cell//this finishes up
#endif  //end of buffer spi
//dont change optimize here, look for value at top
    #if optimize > 0 & colorMode <2
int pixelsbuf[AMG88xx_PIXEL_ARRAY_SIZE];
    #endif
  #if optimize > 0 & colorMode > 1
int pixelsbuf[AMG88xx_PIXEL_ARRAY_SIZE];//we double size of array so it can handle larger numbers for larger color tables
  #endif
    #if interpolatemode >0 //we use it all modes above 0 now.... 
//int postbuffer[AMG88xx_PIXEL_ARRAY_SIZE*4];//this is a temp buffer it wont always be needed -->no loner needed!
byte color4buf;//we use for drawing 4 at a time if enabled, just counts 0 to 3, or 0-15
    #endif
  #if interpolatemode >2 
uint16_t subpixelbuffer2[64];//this is a buffer that holds pixel info to update up to 64  pixesl at a time!
int postbuffer2[AMG88xx_PIXEL_ARRAY_SIZE*16];//this is a temp buffer it wont always be needed
byte color4buf2;//we use for drawing several pixels at a time
  #endif
#if interpolatemode == 2  
uint16_t pixelsubsampledata[16];//we use 16 word buffer to buffer 1 pixel into a 4x4 sub pixel.

#endif 
#if interpolatemode == 3
uint16_t pixelsubsampledata[64];
#endif
#if interpolatemode == 4 //we are going to us custome driver to get performance for this. 
uint16_t pixelsubsampledata[64];
#endif
uint16_t readcolor(uint16_t color){
color=pgm_read_word_near(camColors+color);
return color;
}


uint16_t mainPixel;uint16_t sidePixel;uint16_t topbottomPixel;uint16_t diagnolPixel;//4 values that are used to create sub pixel data
#if interpolatemode == 1  
uint16_t CreateSubPixels(uint16_t pixelColor,uint16_t  sidepixelColor,uint16_t topOrBottomPixelColor,uint16_t diagnalPixelColor){
//this is left with details that shows how it works. more complex method will be more tightly written.  
//[  x+y][  x+1,y] //assume direction is --> if not outside of loop order can be reversed,or flipped top/bottom or left right. needed for all 4 quadrants of screen so no empty data is sampled
//[x+y+1][x+1,y+1] pixels we look at and average                                                                               this prevents absent data and allows all of resolution of data!
// ___   ___  4 pixels sampled and averaged to one pixel location. 
//|x\-| |S  |             [x    ][x+s/2] -->> [x][s]  <-- new subpixel 
//|___| |___|             [x+t/2][x+d/2]      [t][d]
// ___   ___  averaged to 
//|T  | |D  |
//|___| |___|
// ___   this is how we solve, we average main with pixels as shown above
//|   |to:[x][S]  x=main pixelColor,S=sidepixel,T=topbottompixel,D=diagonol pixel
//|___|   [T][D] then we have half averaged sub pixels and each pixel takes 1/4 area of 1st pixel. wala! magic!
//lets do the work and return the data values to main part of loop
mainPixel=pixelColor;// just shown for sanity.compiler should remove it.
sidePixel= (sidepixelColor+pixelColor)/2;//thes are effectively divided. numbers are uint16_t so no need to worry about carry messing it up.
topbottomPixel=(pixelColor+topOrBottomPixelColor)/2;;//ditto
diagnolPixel=(pixelColor+diagnalPixelColor)/2;;//ditto

//now values are averaged from input and output as
//[X][S]
//[T][D]

}
#endif
#if interpolatemode == 2  
uint16_t CreateSubPixels4x4(uint16_t pixelColor,uint16_t  sidepixelColor,uint16_t topOrBottomPixelColor,uint16_t diagnalPixelColor,bool xcenter, bool ycenter){
  //we need to switch pixel order of center screen pixels because of quadrunt sub sampling. this is needed because here the pixel densitt increases as for chance of error. artifacts will appear
// side pixels data wil be different during center pixel loop
//this is too complex to assign regular variables, we switch to an array of 16 2byte pixels. the corner data needs to be raw info. most data needs to be thru processed pixels
//[x][s] [S][3][x1  ]//large boxes are raw actual pixels. we sub process these into partial smaller pixels to get information
//[t][d] [6][7][.   ]// break down raw pixel into sub pixesl (x,and .) x is raw pixel data, '.' is averaged data. this is
//                   //how pixel information is sub sampled outside of normal pixel area. this new info allows proper sub sample
//[T][9] [A][B][x2  ]
//[C][D] [E][F][.   ]
//
//[x3. ] [x4. ][x5 .]
//[    ] [    ][    ]
mainPixel=pixelColor;// just shown for sanity.compiler should remove it.
sidePixel= (sidepixelColor+pixelColor)/2;//thes are effectively divided. numbers are uint16_t so no need to worry about carry messing it up.
topbottomPixel=(pixelColor+topOrBottomPixelColor)/2;;//ditto
diagnolPixel=(pixelColor+diagnalPixelColor)/2;;//ditto
uint16_t x1,x2,x3,x4,x5=0;

if (!xcenter ){
x1= sidepixelColor;//this shows how we solve for other pixels with partial information. code should be reduced from compiler
x2= (sidepixelColor+diagnalPixelColor)/2;
x5= diagnalPixelColor;
}else{
x1= (sidePixel+sidePixel+sidePixel/2+sidePixel/4+sidepixelColor+sidepixelColor/4)/4;//this shows how we solve for other pixels with partial information. code should be reduced from compiler
x2= (sidePixel+sidePixel+sidePixel/2+sidePixel/4+(sidepixelColor+diagnalPixelColor)/2+(sidepixelColor+diagnalPixelColor)/8)/4;
x5=  (diagnalPixelColor+diagnolPixel)/2;//(diagnalPixelColor/4+mainPixel+mainPixel+mainPixel+mainPixel/2+mainPixel/4)/4; 
}
if (!ycenter ){
x3= topOrBottomPixelColor;
x4= (topOrBottomPixelColor+diagnalPixelColor)/2;
x5= (diagnalPixelColor+diagnolPixel)/2;//(diagnalPixelColor/4+mainPixel+mainPixel+mainPixel+mainPixel/2+mainPixel/4)/4;  
}else{
x3= (topbottomPixel+topbottomPixel+topbottomPixel+topbottomPixel/2+topbottomPixel/4+topbottomPixel/8+topOrBottomPixelColor/8)/4;
x4=(topbottomPixel+topbottomPixel+topbottomPixel+topbottomPixel/2+topbottomPixel/4+topbottomPixel/8+(topOrBottomPixelColor+diagnalPixelColor)/16)/4;
x5= (diagnalPixelColor+diagnolPixel)/2;//(diagnalPixelColor/4+mainPixel+mainPixel+mainPixel+mainPixel/2+mainPixel/4)/4;  
}

pixelsubsampledata[0]=mainPixel; 
pixelsubsampledata[1]=(mainPixel+sidePixel)/2;
pixelsubsampledata[4]=(mainPixel+topbottomPixel)/2 ;
pixelsubsampledata[5]=(mainPixel+diagnolPixel)/2;

pixelsubsampledata[2]=sidePixel;
pixelsubsampledata[3]=(sidePixel+x1)/2;                                          
pixelsubsampledata[6]=(sidePixel+diagnolPixel)/2; 
pixelsubsampledata[7]=(sidePixel+x2)/2;

pixelsubsampledata[8]=topbottomPixel;
pixelsubsampledata[9]=(topbottomPixel+diagnolPixel)/2 ;                                
pixelsubsampledata[12]=(topbottomPixel+x3)/2 ;                                                                                           
pixelsubsampledata[13]=(topbottomPixel+x4)/2;

pixelsubsampledata[10]=diagnolPixel;
pixelsubsampledata[11]=(diagnolPixel+x2)/2;
pixelsubsampledata[14]=(diagnolPixel+x4)/2;
pixelsubsampledata[15]=(diagnolPixel+x5)/2;

#if enhancedDetail == true
//please dont change. this was done with experimentation so changes would not look pixelated
#define amountc -15 //enhance around edges
#define amount 20 //this is amount of enhancement if color curve in a negative
#define amounti 9 //this is amount of enhancement if color curve in posative
#define pop 3 //if individual pixel different we pop it (without looking at color curve)
uint16_t center0=(pixelsubsampledata[0]+pixelsubsampledata[1]+pixelsubsampledata[4]+pixelsubsampledata[5])/4;
uint16_t center1=(pixelsubsampledata[2]+pixelsubsampledata[3]+pixelsubsampledata[6]+pixelsubsampledata[7])/4;
uint16_t center2=(pixelsubsampledata[8]+pixelsubsampledata[9]+pixelsubsampledata[12]+pixelsubsampledata[13])/4;
uint16_t center3=(pixelsubsampledata[10]+pixelsubsampledata[11]+pixelsubsampledata[14]+pixelsubsampledata[15])/4;
if (center0>pixelsubsampledata[0]) {pixelsubsampledata[0]=pixelsubsampledata[0]-amount;}else{pixelsubsampledata[0]=pixelsubsampledata[0]+amounti;};
if (center0>pixelsubsampledata[1]) {pixelsubsampledata[1]=pixelsubsampledata[1]-amount;}else{pixelsubsampledata[1]=pixelsubsampledata[1]+amounti;};
if (center0>pixelsubsampledata[4]) {pixelsubsampledata[4]=pixelsubsampledata[4]-amount;}else{pixelsubsampledata[4]=pixelsubsampledata[4]+amounti;};
if (center0>pixelsubsampledata[5]) {pixelsubsampledata[5]=pixelsubsampledata[5]-amount;}else{pixelsubsampledata[5]=pixelsubsampledata[5]+amounti;};

if (center1>pixelsubsampledata[2]) {pixelsubsampledata[2]=pixelsubsampledata[2]-amount;}else{pixelsubsampledata[2]=pixelsubsampledata[2]+amounti;};
if (center1>pixelsubsampledata[3]) {pixelsubsampledata[3]=pixelsubsampledata[3]-amount;}else{pixelsubsampledata[3]=pixelsubsampledata[3]+amounti;};
if (center1>pixelsubsampledata[6]) {pixelsubsampledata[6]=pixelsubsampledata[6]-amount;}else{pixelsubsampledata[6]=pixelsubsampledata[6]+amounti;};
if (center1>pixelsubsampledata[7]) {pixelsubsampledata[7]=pixelsubsampledata[7]-amount;}else{pixelsubsampledata[7]=pixelsubsampledata[7]+amounti;};

if (center2>pixelsubsampledata[8]) {pixelsubsampledata[8]=pixelsubsampledata[8]-amount;}else{pixelsubsampledata[8]=pixelsubsampledata[8]+amounti;};
if (center2>pixelsubsampledata[9]) {pixelsubsampledata[9]=pixelsubsampledata[9]-amount;}else{pixelsubsampledata[9]=pixelsubsampledata[9]+amounti;};
if (center2>pixelsubsampledata[12]) {pixelsubsampledata[12]=pixelsubsampledata[12]-amount;}else{pixelsubsampledata[12]=pixelsubsampledata[12]+amounti;};
if (center2>pixelsubsampledata[13]) {pixelsubsampledata[13]=pixelsubsampledata[13]-amount;}else{pixelsubsampledata[13]=pixelsubsampledata[13]+amounti;};

if (center3>pixelsubsampledata[10]) {pixelsubsampledata[10]=pixelsubsampledata[10]-amount;}else{pixelsubsampledata[10]=pixelsubsampledata[10]+amounti;};
if (center3>pixelsubsampledata[11]) {pixelsubsampledata[11]=pixelsubsampledata[11]-amount;}else{pixelsubsampledata[11]=pixelsubsampledata[11]+amounti;};
if (center3>pixelsubsampledata[14]) {pixelsubsampledata[14]=pixelsubsampledata[14]-amount;}else{pixelsubsampledata[14]=pixelsubsampledata[14]+amounti;};
if (center3>pixelsubsampledata[15]) {pixelsubsampledata[15]=pixelsubsampledata[15]-amount;}else{pixelsubsampledata[15]=pixelsubsampledata[15]+amounti;};
pixelsubsampledata[5]-=amountc/2;pixelsubsampledata[6]-=amountc/2;pixelsubsampledata[9]-=amountc/2;pixelsubsampledata[10]-=amountc/2;
pixelsubsampledata[2]-=amountc;pixelsubsampledata[3]-=amountc;pixelsubsampledata[4]-=amountc;pixelsubsampledata[5]-=amountc;
pixelsubsampledata[9]-=amountc;pixelsubsampledata[11]-=amountc;pixelsubsampledata[13]-=amountc;pixelsubsampledata[14]-=amountc;
//pixel 5
if (pixelsubsampledata[5]>pixelsubsampledata[4]){pixelsubsampledata[5]+=pop;}if (pixelsubsampledata[5]>pixelsubsampledata[6]){pixelsubsampledata[5]+=pop;}
if (pixelsubsampledata[5]>pixelsubsampledata[1]){pixelsubsampledata[5]+=pop;}if (pixelsubsampledata[5]>pixelsubsampledata[9]){pixelsubsampledata[5]+=pop;}
//pixel 6
if (pixelsubsampledata[6]>pixelsubsampledata[5]){pixelsubsampledata[6]+=pop;}if (pixelsubsampledata[6]>pixelsubsampledata[7]){pixelsubsampledata[6]+=pop;}
if (pixelsubsampledata[6]>pixelsubsampledata[2]){pixelsubsampledata[6]+=pop;}if (pixelsubsampledata[6]>pixelsubsampledata[10]){pixelsubsampledata[6]+=pop;}
//pixel 9
if (pixelsubsampledata[9]>pixelsubsampledata[8]){pixelsubsampledata[9]+=pop;}if (pixelsubsampledata[9]>pixelsubsampledata[10]){pixelsubsampledata[9]+=pop;}
if (pixelsubsampledata[9]>pixelsubsampledata[6]){pixelsubsampledata[9]+=pop;}if (pixelsubsampledata[9]>pixelsubsampledata[14]){pixelsubsampledata[9]+=pop;}
//pixel 10
if (pixelsubsampledata[10]>pixelsubsampledata[9]){pixelsubsampledata[10]+=pop;}if (pixelsubsampledata[10]>pixelsubsampledata[11]){pixelsubsampledata[10]+=pop;}
if (pixelsubsampledata[10]>pixelsubsampledata[6]){pixelsubsampledata[10]+=pop;}if (pixelsubsampledata[10]>pixelsubsampledata[14]){pixelsubsampledata[10]+=pop;}
#endif
}
#endif
#if interpolatemode >2 //we use for 3 and 4 //this is for 8x8 sub sampling. 

 
uint16_t CreateSubPixels8x8(uint16_t pixelColor,uint16_t  sidepixelColor,uint16_t topOrBottomPixelColor,uint16_t diagnalPixelColor,bool xcenter, bool ycenter){ 
// look at other modes for learning how.. it includes xcenter and y center these require different methods as center pixel is [x][s][s][x]. we have all needed
mainPixel=pixelColor;// just shown for sanity.compiler should remove it.                                                      [t][d][d][t] data just we process differently
sidePixel= (sidepixelColor+pixelColor)/2;//thes are effectively divided. numbers are uint16_t so no need to worry about carry messing it up.
topbottomPixel=(pixelColor+topOrBottomPixelColor)/2;;//ditto
diagnolPixel=(pixelColor+diagnalPixelColor)/2;//ditto
uint16_t x1,x2,x3,x4,x5=0;

if (!xcenter ){
x1= sidepixelColor;//this shows how we solve for other pixels with partial information. code should be reduced from compiler
x2= (sidepixelColor+diagnalPixelColor)/2;
x5= diagnalPixelColor;
}else{
x1= (sidePixel+sidePixel+sidePixel/2+sidePixel/4+sidepixelColor+sidepixelColor/4)/4;//this shows how we solve for other pixels with partial information. code should be reduced from compiler
x2= (sidePixel+sidePixel+sidePixel/2+sidePixel/4+(sidepixelColor+diagnalPixelColor)/2+(sidepixelColor+diagnalPixelColor)/8)/4;
x5=  (diagnalPixelColor+diagnolPixel)/2;//(diagnalPixelColor/4+mainPixel+mainPixel+mainPixel+mainPixel/2+mainPixel/4)/4; 
}
if (!ycenter ){
x3= topOrBottomPixelColor;
x4= (topOrBottomPixelColor+diagnalPixelColor)/2;
x5= (diagnalPixelColor+diagnolPixel)/2;//(diagnalPixelColor/4+mainPixel+mainPixel+mainPixel+mainPixel/2+mainPixel/4)/4;  
}else{
x3= (topbottomPixel+topbottomPixel+topbottomPixel+topbottomPixel/2+topbottomPixel/4+topbottomPixel/8+topOrBottomPixelColor/8)/4;
x4=(topbottomPixel+topbottomPixel+topbottomPixel+topbottomPixel/2+topbottomPixel/4+topbottomPixel/8+(topOrBottomPixelColor+diagnalPixelColor)/16)/4;
x5= (diagnalPixelColor+diagnolPixel)/2;//(diagnalPixelColor/4+mainPixel+mainPixel+mainPixel+mainPixel/2+mainPixel/4)/4;  
}


//we first solve at 4x4, then we apply enhancements. then we subsample again to 8x8. it is just easier to test and troubleshoot. compiler should break it down. it seems fast
pixelsubsampledata[0]=mainPixel; 
pixelsubsampledata[1]=(mainPixel+sidePixel)/2;
pixelsubsampledata[4]=(mainPixel+topbottomPixel)/2 ;
pixelsubsampledata[5]=(mainPixel+diagnolPixel)/2;

pixelsubsampledata[2]=sidePixel;
pixelsubsampledata[3]=(sidePixel+x1)/2;                                          
pixelsubsampledata[6]=(sidePixel+diagnolPixel)/2; 
pixelsubsampledata[7]=(sidePixel+x2)/2;

pixelsubsampledata[8]=topbottomPixel;
pixelsubsampledata[9]=(topbottomPixel+diagnolPixel)/2 ;                                
pixelsubsampledata[12]=(topbottomPixel+x3)/2 ;                                                                                           
pixelsubsampledata[13]=(topbottomPixel+x4)/2;

pixelsubsampledata[10]=diagnolPixel;
pixelsubsampledata[11]=(diagnolPixel+x2)/2;
pixelsubsampledata[14]=(diagnolPixel+x4)/2;
pixelsubsampledata[15]=(diagnolPixel+x5)/2;

#if enhancedDetail == true
//please dont change. this was done with experimentation so changes would not look pixelated
#define amountc -5 //enhance around edges
#define amount 10 //this is amount of enhancement if color curve in a negative
#define amounti 5 //this is amount of enhancement if color curve in posative
#define pop 2 //if individual pixel different we pop it (without looking at color curve)
uint16_t center0=(pixelsubsampledata[0]+pixelsubsampledata[1]+pixelsubsampledata[4]+pixelsubsampledata[5])/4;
uint16_t center1=(pixelsubsampledata[2]+pixelsubsampledata[3]+pixelsubsampledata[6]+pixelsubsampledata[7])/4;
uint16_t center2=(pixelsubsampledata[8]+pixelsubsampledata[9]+pixelsubsampledata[12]+pixelsubsampledata[13])/4;
uint16_t center3=(pixelsubsampledata[10]+pixelsubsampledata[11]+pixelsubsampledata[14]+pixelsubsampledata[15])/4;
if (center0>pixelsubsampledata[0]) {pixelsubsampledata[0]=pixelsubsampledata[0]-amount;}else{pixelsubsampledata[0]=pixelsubsampledata[0]+amounti;};
if (center0>pixelsubsampledata[1]) {pixelsubsampledata[1]=pixelsubsampledata[1]-amount;}else{pixelsubsampledata[1]=pixelsubsampledata[1]+amounti;};
if (center0>pixelsubsampledata[4]) {pixelsubsampledata[4]=pixelsubsampledata[4]-amount;}else{pixelsubsampledata[4]=pixelsubsampledata[4]+amounti;};
if (center0>pixelsubsampledata[5]) {pixelsubsampledata[5]=pixelsubsampledata[5]-amount;}else{pixelsubsampledata[5]=pixelsubsampledata[5]+amounti;};

if (center1>pixelsubsampledata[2]) {pixelsubsampledata[2]=pixelsubsampledata[2]-amount;}else{pixelsubsampledata[2]=pixelsubsampledata[2]+amounti;};
if (center1>pixelsubsampledata[3]) {pixelsubsampledata[3]=pixelsubsampledata[3]-amount;}else{pixelsubsampledata[3]=pixelsubsampledata[3]+amounti;};
if (center1>pixelsubsampledata[6]) {pixelsubsampledata[6]=pixelsubsampledata[6]-amount;}else{pixelsubsampledata[6]=pixelsubsampledata[6]+amounti;};
if (center1>pixelsubsampledata[7]) {pixelsubsampledata[7]=pixelsubsampledata[7]-amount;}else{pixelsubsampledata[7]=pixelsubsampledata[7]+amounti;};

if (center2>pixelsubsampledata[8]) {pixelsubsampledata[8]=pixelsubsampledata[8]-amount;}else{pixelsubsampledata[8]=pixelsubsampledata[8]+amounti;};
if (center2>pixelsubsampledata[9]) {pixelsubsampledata[9]=pixelsubsampledata[9]-amount;}else{pixelsubsampledata[9]=pixelsubsampledata[9]+amounti;};
if (center2>pixelsubsampledata[12]) {pixelsubsampledata[12]=pixelsubsampledata[12]-amount;}else{pixelsubsampledata[12]=pixelsubsampledata[12]+amounti;};
if (center2>pixelsubsampledata[13]) {pixelsubsampledata[13]=pixelsubsampledata[13]-amount;}else{pixelsubsampledata[13]=pixelsubsampledata[13]+amounti;};

if (center3>pixelsubsampledata[10]) {pixelsubsampledata[10]=pixelsubsampledata[10]-amount;}else{pixelsubsampledata[10]=pixelsubsampledata[10]+amounti;};
if (center3>pixelsubsampledata[11]) {pixelsubsampledata[11]=pixelsubsampledata[11]-amount;}else{pixelsubsampledata[11]=pixelsubsampledata[11]+amounti;};
if (center3>pixelsubsampledata[14]) {pixelsubsampledata[14]=pixelsubsampledata[14]-amount;}else{pixelsubsampledata[14]=pixelsubsampledata[14]+amounti;};
if (center3>pixelsubsampledata[15]) {pixelsubsampledata[15]=pixelsubsampledata[15]-amount;}else{pixelsubsampledata[15]=pixelsubsampledata[15]+amounti;};
pixelsubsampledata[5]-=amountc/2;pixelsubsampledata[6]-=amountc/2;pixelsubsampledata[9]-=amountc/2;pixelsubsampledata[10]-=amountc/2;
pixelsubsampledata[2]-=amountc;pixelsubsampledata[3]-=amountc;pixelsubsampledata[4]-=amountc;pixelsubsampledata[5]-=amountc;
pixelsubsampledata[9]-=amountc;pixelsubsampledata[11]-=amountc;pixelsubsampledata[13]-=amountc;pixelsubsampledata[14]-=amountc;
//pixel 5
if (pixelsubsampledata[5]>pixelsubsampledata[4]){pixelsubsampledata[5]+=pop;}if (pixelsubsampledata[5]>pixelsubsampledata[6]){pixelsubsampledata[5]+=pop;}
if (pixelsubsampledata[5]>pixelsubsampledata[1]){pixelsubsampledata[5]+=pop;}if (pixelsubsampledata[5]>pixelsubsampledata[9]){pixelsubsampledata[5]+=pop;}
//pixel 6
if (pixelsubsampledata[6]>pixelsubsampledata[5]){pixelsubsampledata[6]+=pop;}if (pixelsubsampledata[6]>pixelsubsampledata[7]){pixelsubsampledata[6]+=pop;}
if (pixelsubsampledata[6]>pixelsubsampledata[2]){pixelsubsampledata[6]+=pop;}if (pixelsubsampledata[6]>pixelsubsampledata[10]){pixelsubsampledata[6]+=pop;}
//pixel 9
if (pixelsubsampledata[9]>pixelsubsampledata[8]){pixelsubsampledata[9]+=pop;}if (pixelsubsampledata[9]>pixelsubsampledata[10]){pixelsubsampledata[9]+=pop;}
if (pixelsubsampledata[9]>pixelsubsampledata[6]){pixelsubsampledata[9]+=pop;}if (pixelsubsampledata[9]>pixelsubsampledata[14]){pixelsubsampledata[9]+=pop;}
//pixel 10
if (pixelsubsampledata[10]>pixelsubsampledata[9]){pixelsubsampledata[10]+=pop;}if (pixelsubsampledata[10]>pixelsubsampledata[11]){pixelsubsampledata[10]+=pop;}
if (pixelsubsampledata[10]>pixelsubsampledata[6]){pixelsubsampledata[10]+=pop;}if (pixelsubsampledata[10]>pixelsubsampledata[14]){pixelsubsampledata[10]+=pop;}
#endif
//since we recycled this from 32x32 mode we need to reasign values                        mainPixel=pixelColor;// just shown for sanity.compiler should remove it.
//[x 0][  1][x 2][  3][x 4][  5][x 6][ 7][x1][  ] --> each x is [x][s]                    sidePixel= (sidepixelColor+pixelColor)/2;//thes are effectively divided. 
//[  8][  9][ 10][ 11][ 12][ 13][ 14][15][  ][  ]               [t][d] pixel data.        topbottomPixel=(pixelColor+topOrBottomPixelColor)/2;;//ditto
//[x16][ 17][x18][ 19][x20][ 21][x22][23][x2][  ]                                         diagnolPixel=(pixelColor+diagnalPixelColor)/2;;//ditto
//[ 24][ 25][ 26][ 27][ 28][ 29][ 30][31][  ][  ]
//[x32][ 33][x34][ 35][x36][ 37][x38][39][x3][  ]     from [ 0][ 1][ 2][ 3][][]
//[ 40][ 41][ 42][ 43][ 44][ 45][ 46][47][  ][  ]          [ 4][ 5][ 6][ 7][][]
//[x48][ 49][x50][ 51][x52][ 53][x54][55][x4][  ]          [ 8][ 9][10][11][][]
//[ 56][ 57][ 58][ 59][ 60][ 61][ 62][63][  ][  ]          [12][13][14][15][][]
//[ x5][   ][x6 ][   ][x7 ][   ][x8 ][  ][x9]              [  ][  ][  ][  ][][]  <--extra pixels we create from side pixel information
//we reasign and have more side data to look at, we need to break it down more as we are comparing more pixels
//we do this first, as data will be overwritted in others as we sub sample, so we copy data over in a way to get proper data. dont want to overwrite first then copy...
pixelsubsampledata[0]=pixelsubsampledata[0];
pixelsubsampledata[18]=pixelsubsampledata[5];

pixelsubsampledata[20]=pixelsubsampledata[6];
pixelsubsampledata[22]=pixelsubsampledata[7];
pixelsubsampledata[32]=pixelsubsampledata[8];
pixelsubsampledata[34]=pixelsubsampledata[9];                             
pixelsubsampledata[48]=pixelsubsampledata[12];                                                          
pixelsubsampledata[50]=pixelsubsampledata[13];
pixelsubsampledata[36]=pixelsubsampledata[10];
pixelsubsampledata[38]=pixelsubsampledata[11];
pixelsubsampledata[52]=pixelsubsampledata[14];
pixelsubsampledata[54]=pixelsubsampledata[15];
//these moved to prevent corrupt data
pixelsubsampledata[16]=pixelsubsampledata[4];
pixelsubsampledata[4]=pixelsubsampledata[2];
pixelsubsampledata[2]=pixelsubsampledata[1];
pixelsubsampledata[6]=pixelsubsampledata[3];


//#define lightoffset 0
//we reasign and have more side data to look at, we need to break it down more as we are comparing more pixels
         x1= sidepixelColor;//this shows how we solve for other pixels with partial information. code should be reduced from compiler

         x2= (sidepixelColor+sidepixelColor+sidepixelColor+diagnalPixelColor)/4;//note that on side pixels we need to calc as blocks in 1/4th.
         x3= (sidepixelColor+sidepixelColor+diagnalPixelColor+diagnalPixelColor)/4;
         x4=(sidepixelColor+diagnalPixelColor+diagnalPixelColor+diagnalPixelColor)/4;
         x5=topOrBottomPixelColor;
uint16_t x6=(topOrBottomPixelColor+topOrBottomPixelColor+topOrBottomPixelColor+diagnalPixelColor)/4;
uint16_t x7=(topOrBottomPixelColor+topOrBottomPixelColor+diagnalPixelColor+diagnalPixelColor)/4;
uint16_t x8=(topOrBottomPixelColor+diagnalPixelColor+diagnalPixelColor+diagnalPixelColor)/4;
uint16_t x9=diagnalPixelColor;//these are solved partial pixel. easier to insert this than code below. compiler wil simplify it to raw form, wich is too messy for me to troubleshoot

//upper //left //here is where sub sample is solved

pixelsubsampledata[1]=(pixelsubsampledata[0]+pixelsubsampledata[2] )/2;
pixelsubsampledata[8]=(pixelsubsampledata[0]+pixelsubsampledata[16] )/2;
pixelsubsampledata[9]=(pixelsubsampledata[0]+pixelsubsampledata[18] )/2;

pixelsubsampledata[3]=(pixelsubsampledata[2]+pixelsubsampledata[4] )/2;
pixelsubsampledata[10]=(pixelsubsampledata[2]+pixelsubsampledata[18] )/2;
pixelsubsampledata[11]=(pixelsubsampledata[2]+pixelsubsampledata[20] )/2;

pixelsubsampledata[17]=(pixelsubsampledata[16]+pixelsubsampledata[18] )/2;
pixelsubsampledata[24]=(pixelsubsampledata[16]+pixelsubsampledata[32] )/2;
pixelsubsampledata[25]=(pixelsubsampledata[16]+pixelsubsampledata[34] )/2;

pixelsubsampledata[19]=(pixelsubsampledata[18]+pixelsubsampledata[20] )/2;
pixelsubsampledata[26]=(pixelsubsampledata[18]+pixelsubsampledata[34])/2;
pixelsubsampledata[27]=(pixelsubsampledata[18]+pixelsubsampledata[36] )/2;

//lower
pixelsubsampledata[33]=(pixelsubsampledata[32]+pixelsubsampledata[34] )/2;
pixelsubsampledata[40]=(pixelsubsampledata[32]+pixelsubsampledata[48] )/2;
pixelsubsampledata[41]=(pixelsubsampledata[32]+pixelsubsampledata[50] )/2;

pixelsubsampledata[35]=(pixelsubsampledata[34]+pixelsubsampledata[36] )/2;
pixelsubsampledata[42]=(pixelsubsampledata[34]+pixelsubsampledata[50] )/2;
pixelsubsampledata[43]=(pixelsubsampledata[34]+pixelsubsampledata[52] )/2;

pixelsubsampledata[49]=(pixelsubsampledata[48]+pixelsubsampledata[50] )/2;
pixelsubsampledata[56]=(pixelsubsampledata[48]+x5 )/2;
pixelsubsampledata[57]=(pixelsubsampledata[48]+x6 )/2;

pixelsubsampledata[51]=(pixelsubsampledata[50]+pixelsubsampledata[52] )/2;
pixelsubsampledata[58]=(pixelsubsampledata[50]+x6 )/2;
pixelsubsampledata[59]=(pixelsubsampledata[50]+x7 )/2;

//upper //right //
pixelsubsampledata[5]=(pixelsubsampledata[4]+pixelsubsampledata[6] )/2;
pixelsubsampledata[12]=(pixelsubsampledata[4]+pixelsubsampledata[20] )/2;
pixelsubsampledata[13]=(pixelsubsampledata[4]+pixelsubsampledata[22] )/2;

pixelsubsampledata[7]=(pixelsubsampledata[6] +x1)/2;
pixelsubsampledata[14]=(pixelsubsampledata[6]+pixelsubsampledata[22] )/2;                              
pixelsubsampledata[15]=(pixelsubsampledata[6]+x2 )/2;

pixelsubsampledata[21]=(pixelsubsampledata[20]+pixelsubsampledata[22] )/2;
pixelsubsampledata[28]=(pixelsubsampledata[20]+pixelsubsampledata[36] )/2;
pixelsubsampledata[29]=(pixelsubsampledata[20]+pixelsubsampledata[38] )/2;

pixelsubsampledata[23]=(pixelsubsampledata[22]+x2 )/2;
pixelsubsampledata[30]=(pixelsubsampledata[22]+pixelsubsampledata[38] )/2;
pixelsubsampledata[31]=(pixelsubsampledata[22]+x3 )/2;

//lower
pixelsubsampledata[37]=(pixelsubsampledata[36]+pixelsubsampledata[38] )/2;
pixelsubsampledata[44]=(pixelsubsampledata[36]+pixelsubsampledata[52] )/2;
pixelsubsampledata[45]=(pixelsubsampledata[36]+pixelsubsampledata[54] )/2;

pixelsubsampledata[39]=(pixelsubsampledata[38]+x3 )/2;
pixelsubsampledata[46]=(pixelsubsampledata[38]+pixelsubsampledata[54] )/2;
pixelsubsampledata[47]=(pixelsubsampledata[38]+x4 )/2;

pixelsubsampledata[53]=(pixelsubsampledata[52]+pixelsubsampledata[54] )/2;
pixelsubsampledata[60]=(pixelsubsampledata[52]+x7 )/2;
pixelsubsampledata[61]=(pixelsubsampledata[52]+x8 )/2;

pixelsubsampledata[55]=(pixelsubsampledata[54]+x4 )/2;
pixelsubsampledata[62]=(pixelsubsampledata[54]+x8 )/2;
pixelsubsampledata[63]=(pixelsubsampledata[54]+x9 )/2;

//side pixel 
}
#endif
//(topOrBottomPixelColor+diagnalPixelColor)+diagnalPixelColor) sidepixelColor  //for cut and paste remove later on
void setup() {

 // Serial.begin(9600);
 Serial.begin(115200);Serial.println(F("AMG88xx thermal camera!"));
    tft.initR(INITR_144GREENTAB);   // initialize a ST7735S chip, black tab
    tft.fillScreen(ST7735_BLACK);

    bool status;    status = amg.begin();
    if (!status) {Serial.println(F("Could not find a valid AMG88xx sensor, check wiring!"));while (1);}
    Serial.println( F("-- Thermal Camera Test --")); delay(100); // let sensor boot up
}


// friends it may be magical bologna, but it speeds things up quite abit!
//it looks at buffered value, and only updates the greated changes pixel locations
//and once changed, the next sets of pixels get updated. this reduces noise and 
//increases data thruput to display by only updating if change, and if overloaded 
//by most change. faster performance if fewer pixels set to priority                          
//      data range  -->     ||  limit data  ||Fuzzy logic data reduction                                           
//=========================]|| to greatest  ||==========================
//   bandwith compressor    ||    changed   || (by james villeneuve 2018)
#define speedUpCompression 8 //lower number is faster (it sets priority pixels amount, however too much means rest of display updates more slowly)
// also includes code to make sure every pixel updates but at a slower rate 1/8 or 1/16 think of it like a keyframe that updates 1/16 of display at a time no matter what  setting is

//higher number precision means more change allowed for pixel noise

//noise suppression, what shows at lower limits goes to lowest color 
int compressionnumber;//this counts pixels processed, if to many processed we make compression flux higher
int compressionflux=50;//this number goes up and down depending on how many pixesl are changing (usually from noise)

float cachedTempVar;//we use for comparing, /********** faster than map data. normally it is bad forum to compress into single line, but this code takes up space!
float cachedtempdata;//this is data we reuse!!  //we only recalc values if they have changed!
float Fasttempcachemap256(float valuechange){if (cachedTempVar !=MAXTEMP+MINTEMP){cachedtempdata=256/(MAXTEMP-MINTEMP);cachedTempVar =MAXTEMP+MINTEMP;};valuechange=(valuechange-MINTEMP)*cachedTempVar;return valuechange;}
float Fasttempcachemap1024(float valuechange){if (cachedTempVar !=MAXTEMP+MINTEMP){cachedtempdata=1024/(MAXTEMP-MINTEMP);cachedTempVar =MAXTEMP+MINTEMP;};valuechange=(valuechange-MINTEMP)*cachedTempVar;return valuechange;}
int colorRangeSet(int colordata, float Value){
  #if colorMode <2 
  colordata= Fasttempcachemap256(Value);
 #else
 colordata=Fasttempcachemap1024( Value);//we constrain color after subsampling
 #endif
return colordata;
                                              }
int colorClamp(int colordata){
if (colordata<threshold){ colordata=0;}
#if colorMode < 2
 colordata= colordata &255;//subsample with real pixel and surounding pixels
#else
 colordata= colordata&1023;//subsample with real pixel and surounding pixels
#endif

return colordata;
                              }
void displayaddons(){//this area is code that updates display with numeric data and shows color bar if set
#if showcolorbar == true
       #if temp_Fahrenheit == true
byte tempf=MAXTEMP*1.8+32;byte charplace1a= tempf /10;byte charplace2a= tempf-charplace1a*10;//we have 2 digit limits
       #endif
       #if temp_Fahrenheit == false
byte tempf=MAXTEMP;byte charplace1a= tempf /10;byte charplace2a= tempf-charplace1a*10;// we have 2 digits limits
      #endif
if (tempf<100){tft.drawChar(128-22,128-13,48+charplace1a,0xFFFF,0x0000,2);tft.drawChar(128-11,128-13,48+charplace2a,0xFFFF,0x0000,2);
}else{tft.drawChar(128-11,128-13,65,0xFFFF,0x0000,2);tft.drawChar(128-22,128-13,78,0xFFFF,0x0000,2);////draw NA
}
      #if temp_Fahrenheit == true
byte tempf2=MINTEMP*1.8+32;byte charplace1b= tempf2 /10;byte charplace2b= tempf2-charplace1b*10;//we have 2 digits
      #endif
      #if temp_Fahrenheit == false
byte tempf2=MINTEMP;byte charplace1b= tempf2 /10;byte charplace2b= tempf2-charplace1b*10;//we have 2 digits
      #endif
if (tempf2>0){tft.drawChar(0,128-13,48+charplace1b,0xFFFF,0x0000,2);tft.drawChar(11,128-13,48+charplace2b,0xFFFF,0x0000,2);
}else{tft.drawChar(11,128-13,65,0xFFFF,0x0000,2);tft.drawChar(0,128-13,78,0xFFFF,0x0000F,2);}//draw NA
tft.drawRect(24,  128-13,80,  10,0x0000) ;float range=MAXTEMP-MINTEMP;float stepcolorchange=range*0.025;float colorstepping=MINTEMP;// we draw  box outline for color bar and setup color range of color bar
int colorOfbararea=0;for (int drawbar=0;drawbar<40;drawbar++){colorOfbararea=colorRangeSet(colorOfbararea,colorstepping);tft.fillRect(24+drawbar*2, 128-12,2, 8,(uint16_t)pgm_read_word_near(camColors+colorOfbararea));colorstepping+= stepcolorchange; } //color bar draw!
#endif
//this is last area before refresh most the code here and below is just for temp readout, averaging middle 4 data.
#if show_temp_readout == true 
      #if temp_Fahrenheit == true
int temperature= (pixels[8*4+4]*1.8+32+pixels[8*4+5]*1.8+32+pixels[8*3+4]*1.8+32+pixels[8*3+5]*1.8+32)/4;// we sample 4 samples because center of screen is 4 samples
      #else
int temperature= (pixels[8*4+4]+pixels[8*4+5]+pixels[8*3+4]+pixels[8*3+5])/4;// we sample 4 samples because center of screen is 4 samples
      #endif
if (temperature >0 & temperature<100){byte charplace1=temperature/10;byte charplace2=temperature-charplace1*10;tft.drawChar(64-11,64+5,48+charplace1,0xFFFF,0xFFFF,2);tft.drawChar(64-10+11,64+5,48+charplace2,0xFFFF,0xFFFF,2);//we get 2 digits
}else{if (temperature>99){tft.drawChar(64-11,64+5,78,0xFFFF,0xFFFF,2);tft.drawChar(64-10+11,64+5,65,0xFFFF,0xFFFF,2);////we put 'NA' on screen if out of range of 2 digits (for now)
}};tft.drawChar(60,60,111,0xFFFF,0xFFFF,1);////we print a little 'o' faster than drawing a circle.
#endif

}

byte slowupdate=0;//used to refesh different amounts, makes sure pixels update more timely. solves noisy pixels

long Hztimer=0;
void loop() {

amg.readPixels(pixels); // if set to zero we just run as fast as possible

#if autorange == true //this is for autorange we turn MINTEMP and MAXTEMP into variables and adjust mixed locations for faster. we look at min and max over all range and adjust over a few samples to avoid crazy.
float templow=1024;float temphi=0;
for (int counti=0;counti<63; counti+=1)              {
if (pixels[counti]>temphi){temphi=pixels[counti];}//some sort of crude bubble sort but only one loop thru....
if (pixels[counti]<templow){templow=pixels[counti];} };
MINTEMP=MINTEMP*0.5+(templow-0.85)*0.5;// 
MAXTEMP=MAXTEMP*0.5+(temphi+0.85)*0.5;
#endif
//*********
float tempr=0.0;byte i=0;int j=0; byte k=0;//long timer =micros(); //values used for main code. timer is for testing time of loops. normally it is remarked out. may do debug loop later on...
compressionnumber=0;slowupdate++;slowupdate=slowupdate&15; //delay(100);// we reset compressionnumber each time thru. delay is there for troubleshooting. may add debug feature later on...
#if interlaced != true 
while(k<8 ){ switch(k){case 0: j=0; break;case 1: j=1; break;case 2: j=2; break;case 3: j=3; break;case 4: j=4; break;case 5: j=5; break;case 6: j=6; break;case 7: j=7; break;}// this allows j to do interleaving of page smoother for eyes and data managment...
if (i>7){i=0;k++;pixfy=0;}//here we run code that makes sense of supersubsamples, sample is done when i=8,k=16
#else 
while(k<8 ){ switch(k){case 0: j=0; break;case 1: j=2; break;case 2: j=4; break;case 3: j=6; break;case 4: j=1; break;case 5: j=3; break;case 6: j=5; break;case 7: j=7; break;}// this allows j to do interleaving of page smoother for eyes and data managment...

if (i>7){i=0;k++;pixfy=0;}//here we run code that makes sense of supersubsamples, sample is done when i=8,k=16
#endif
long timer=micros();//used for testing should be remarked out at some point
    int colorIndex  =  Fasttempcachemap1024( pixels[i+j*8]);//we get data from sensor pixel
   
                #if optimize>1 //we now compress and only update pixels on screen that change the most! it works and over time they all change, but to be sure force updates
                   #if noisefilter > 0 & optimize == 2 //optimise needs to be set to 2 for noise filter to be active
if (pixelsbuf[i+j*8] -colorIndex >noisefilter || colorIndex-pixelsbuf[i+j*8]>noisefilter){
if  ((pixelsbuf[i+j*8] -colorIndex)>compressionflux ||(colorIndex-pixelsbuf[i+j*8]>compressionflux ||i+j*8==(i+j*8)|slowupdate )){
                   #endif
                   #if noisefilter == 0  // if noise filter zero we run a slightly simpler filter to determine if pixel location should be updated
if  ((pixelsbuf[i+j*8] -colorIndex)>compressionflux ||(colorIndex-pixelsbuf[i+j*8]>compressionflux ||i+j*8==(i+j*8)|slowupdate )){//loop with no noise filter code
                   #endif
                #endif    
          #if optimize >0 //this means we use a buffer! 
if (colorIndex !=pixelsbuf[i+j*8]){//we update each pixel that has changed from what is in buffer and we send to display! 
pixelsbuf[i+j*8] =colorIndex;//we just make sure latest pixel value gets stored here
         #endif 
          #if optimize >1    //if enables we also prioritize the most changed pixels to reduce cpu load. rather than frame rate drop we update remainder next few frames, or on a key update as every few locations are updated no matter what in case of noise issues
if (i+j*8 !=(i+j*8)|slowupdate)    compressionnumber++;// we only count priority pixels, not ones with slower refresh
          #endif    
          #if  showcolorbar == true 
if ( i < 7 ){ //we dont write last line of data for now. this is for color bar effect and only if it is enabled. pixel data is still averaged just not displayed where color bar is  
          #endif
    #if interpolatemode == 0
colorIndex=colorClamp(colorIndex);//this is original for display code pixel placement
         #if  subpixelcoloroptimized !=-1 //we test without lcd writes with -1
    tft.fillRect(displayPixelWidth *j, displayPixelHeight * i,displayPixelWidth, displayPixelHeight,(uint16_t)pgm_read_word_near(camColors+colorIndex));
         #endif
    #endif



#if interpolatemode > 0  //will leave here until code is simplified/////////////////////////////////begin of loop unroll///////////////////////////////////// 
int xDir; int yDir; uint16_t tempcolor;int offsetcalx =0;int offsetcaly =0;//we init here. seems to make quadrunt thing less messy
bool xcenter,ycenter;//this is to help post processor know if along center axis
if (j==3 || j==4){xcenter=true;}else{xcenter=false;}
if (i==3 || i==4){ycenter=true;}else{ycenter=false;}
if (j<4){xDir=1;}else{xDir=-1;}
if (i<4){yDir=1;}else{yDir=-1;}
//here we get [x][s]
//            [t][d]
//it runs faster here rather than being in a loop. each method id for 1 quadrunt of display. this keeps all areas with more detail.

mainPixel=Fasttempcachemap1024(pixels[i+j*8]);
sidePixel=Fasttempcachemap1024(pixels[i+(j+xDir)*8]);
topbottomPixel=Fasttempcachemap1024(pixels[i+yDir+j*8]);
diagnolPixel=Fasttempcachemap1024(pixels[(i+yDir)+(j+xDir)*8]);

#if optics_correction == true //this accounts for differences in pixel location measurement from viewing angle differences. so we compare to niebor sensors
#define optical_adjust 4  //best if 2,4,8,16.....
#define boost 10
mainPixel+=boost-Fasttempcachemap1024(pixels[i+yDir+(j+xDir)*8])/optical_adjust;
sidePixel+=boost-Fasttempcachemap1024(pixels[i+(j+xDir+xDir)*8])/optical_adjust;
topbottomPixel+=boost-Fasttempcachemap1024(pixels[i+yDir+yDir+j*8])/optical_adjust;
diagnolPixel+=boost-Fasttempcachemap1024(pixels[(i+yDir+yDir)+(j+xDir+xDir)*8])/optical_adjust;
mainPixel= colorClamp(mainPixel);sidePixel=  colorClamp(sidePixel);topbottomPixel= colorClamp(topbottomPixel);diagnolPixel= colorClamp(diagnolPixel);//clamp colors 0-number

#endif

if (xDir==-1 & yDir==1){ offsetcalx=displayPixelWidth/2;};
if (xDir==1 & yDir==-1){offsetcaly=displayPixelHeight/2;};
if (xDir==-1 & yDir==-1){offsetcaly=displayPixelHeight/2;offsetcalx=displayPixelWidth/2;};

#endif  //interpolatemode//***************************************************************************************************************************************************************

#if interpolatemode == 1
CreateSubPixels( mainPixel, sidePixel,topbottomPixel,diagnolPixel);//magic happens data is returned ready 
#endif
#if interpolatemode == 2
CreateSubPixels4x4( mainPixel, sidePixel,topbottomPixel,diagnolPixel,xcenter,ycenter);//magic happens but data is large enough it is returned in array. we allow enhanced mode if it is set
#endif
#if interpolatemode >2 //we use for 3 and 4
CreateSubPixels8x8( mainPixel, sidePixel,topbottomPixel,diagnolPixel,xcenter,ycenter);//magic happens but data is large enough it is returned in array. we allow enhanced mode if it is set
#endif

#if  interpolatemode > 0 
#define halfdisplayPixelHeight displayPixelHeight/2
#define halfdisplayPixelWidth displayPixelWidth/2

//we move this here as it makes more sense as new routine changes above code significantly
int raster_x=0;while (raster_x !=(xDir+xDir)){ //done with != instead of <> so i could invert direction also while loops use shorter jmp instruction or at least used too. seems faster still! 
int raster_y=0;while (raster_y !=  yDir+yDir) { //0,1  
//if (raster_x==-1) {offsetcalx=16;} 
//if (raster_y==-1) {offsetcaly=0;} 

if (raster_x==0 & raster_y==0){tempcolor= mainPixel;}
if (raster_x==xDir  & raster_y==0){tempcolor=sidePixel;}
if (raster_x==0 & raster_y==yDir ){tempcolor=topbottomPixel;}
if (raster_x==xDir & raster_y==yDir ){tempcolor=diagnolPixel;}
#endif //interpolate mode
#if interpolatemode == 1 //we go ahead and draw stuff
        #if  subpixelcoloroptimized == 0 //this area will not run if set to -1, or if above 1
fillRectFast(displayPixelWidth *j+offsetcalx+(xDir*halfdisplayPixelWidth)*(raster_x*xDir),displayPixelHeight* i+offsetcaly+(yDir*halfdisplayPixelHeight)*(raster_y*yDir),halfdisplayPixelWidth,halfdisplayPixelHeight,
(uint16_t)pgm_read_word_near(camColors+tempcolor));  //we update pixel location with new subsampled pixel.
        #endif  
        #if  subpixelcoloroptimized > 0 //we can run 4 at a time. this is max that can run in this loop, because entire loop uses 4 pixels
        subpixelbuffer[color4buf]=pgm_read_word_near(camColors+tempcolor);//we go ahead and store data that would go to screen in buffer and do a multiwrite at once
        color4buf++;//line below only runs one time per loop
        if (color4buf>3){color4buf=0;//we reset counter, and send all at the same time
        uint16_t data0, data1, data2, data3;//we create this data for manipulations
      
        if (xDir ==1 & yDir==1){data0=subpixelbuffer[0];data1=subpixelbuffer[2];data2=subpixelbuffer[1];data3=subpixelbuffer[3];}
        if (xDir ==-1 & yDir==1){data0=subpixelbuffer[2];data1=subpixelbuffer[0];data2=subpixelbuffer[3];data3=subpixelbuffer[1];}
        if (xDir ==1 & yDir==-1){data0=subpixelbuffer[1];data1=subpixelbuffer[3];data2=subpixelbuffer[0];data3=subpixelbuffer[2];}
        if (xDir ==-1 & yDir==-1){data0=subpixelbuffer[3];data1=subpixelbuffer[1];data2=subpixelbuffer[2];data3=subpixelbuffer[0];}
        tft.fillRectFast4colors(displayPixelWidth *j,displayPixelHeight* i,displayPixelWidth,displayPixelHeight,data0,data1,data2,data3);}
        #endif   
#endif //if interpolate==1 and we stop at that level end


#if interpolatemode >0
raster_y +=  yDir;
   }//interpolatepixel_y 

   raster_x +=xDir;
}//interpolatepixel_x 
#endif
#if interpolatemode ==2

#if subpixelcoloroptimized >0
//here is where low memory 32x32 buffer goes! only uses 16 + 4 words or 40 bytes instead of 512
 if (xDir ==1 & yDir==1){
tft.fillRectFast16colors(displayPixelWidth *j,displayPixelHeight* i,displayPixelWidth,displayPixelHeight,
                          readcolor(pixelsubsampledata[0]),readcolor(pixelsubsampledata[1]),readcolor(pixelsubsampledata[2]),readcolor(pixelsubsampledata[3]));
tft.fillRectFast16colors1(readcolor(pixelsubsampledata[4]),readcolor(pixelsubsampledata[5]),readcolor(pixelsubsampledata[6]),readcolor(pixelsubsampledata[7]));
tft.fillRectFast16colors2(readcolor(pixelsubsampledata[8]),readcolor(pixelsubsampledata[9]),readcolor(pixelsubsampledata[10]),readcolor(pixelsubsampledata[11]));
tft.fillRectFast16colors3(readcolor(pixelsubsampledata[12]),readcolor(pixelsubsampledata[13]),readcolor(pixelsubsampledata[14]),readcolor(pixelsubsampledata[15]));                                                                              
 }

 if (xDir ==1 & yDir==-1){
tft.fillRectFast16colors(displayPixelWidth *j,displayPixelHeight* i,displayPixelWidth,displayPixelHeight,
                          readcolor(pixelsubsampledata[12]),readcolor(pixelsubsampledata[13]),readcolor(pixelsubsampledata[14]),readcolor(pixelsubsampledata[15]));
tft.fillRectFast16colors1(readcolor(pixelsubsampledata[8]),readcolor(pixelsubsampledata[9]),readcolor(pixelsubsampledata[10]),readcolor(pixelsubsampledata[11])); 
tft.fillRectFast16colors2(readcolor(pixelsubsampledata[4]),readcolor(pixelsubsampledata[5]),readcolor(pixelsubsampledata[6]),readcolor(pixelsubsampledata[7]));
tft.fillRectFast16colors3( readcolor(pixelsubsampledata[0]),readcolor(pixelsubsampledata[1]),readcolor(pixelsubsampledata[2]),readcolor(pixelsubsampledata[3]));                                                                             
 }
  if (xDir ==-1 & yDir==1){
tft.fillRectFast16colors(displayPixelWidth *j,displayPixelHeight* i,displayPixelWidth,displayPixelHeight,
                          readcolor(pixelsubsampledata[3]),readcolor(pixelsubsampledata[2]),readcolor(pixelsubsampledata[1]),readcolor(pixelsubsampledata[0]));
tft.fillRectFast16colors1(readcolor(pixelsubsampledata[7]),readcolor(pixelsubsampledata[6]),readcolor(pixelsubsampledata[5]),readcolor(pixelsubsampledata[4]));
tft.fillRectFast16colors2(readcolor(pixelsubsampledata[11]),readcolor(pixelsubsampledata[10]),readcolor(pixelsubsampledata[9]),readcolor(pixelsubsampledata[8]));
tft.fillRectFast16colors3(readcolor(pixelsubsampledata[15]),readcolor(pixelsubsampledata[14]),readcolor(pixelsubsampledata[13]),readcolor(pixelsubsampledata[12]));                                                                              
 }

 if (xDir ==-1 & yDir==-1){
tft.fillRectFast16colors(displayPixelWidth *j,displayPixelHeight* i,displayPixelWidth,displayPixelHeight,
                          readcolor(pixelsubsampledata[15]),readcolor(pixelsubsampledata[14]),readcolor(pixelsubsampledata[13]),readcolor(pixelsubsampledata[12]));
tft.fillRectFast16colors1(readcolor(pixelsubsampledata[11]),readcolor(pixelsubsampledata[10]),readcolor(pixelsubsampledata[9]),readcolor(pixelsubsampledata[8])); 
tft.fillRectFast16colors2(readcolor(pixelsubsampledata[7]),readcolor(pixelsubsampledata[6]),readcolor(pixelsubsampledata[5]),readcolor(pixelsubsampledata[4]));
tft.fillRectFast16colors3( readcolor(pixelsubsampledata[3]),readcolor(pixelsubsampledata[2]),readcolor(pixelsubsampledata[1]),readcolor(pixelsubsampledata[0]));                                                                             
 }
#endif  //subpixelcoloroptimized

#endif  //interpolate mode
#if interpolatemode ==3  //this is for minimal buffer 64x64 mode of pixel writes
#if spi_buffer_local == true
while(!(SPSR & (1<<SPIF) ));
SpiFinAndFinalLcdCommandsForWrite();//this allows buffer to empty out and all spi data writes to complete, including finilization of lcd commands. so we dont confuse lcd chip
#endif
 
#if subpixelcoloroptimized >0
byte c0,c1,c2,c3,c4,c5,c6,c7;
if (yDir==1){if (xDir ==1 ){c0=0;c1=1;c2=2;c3=3;c4=4;c5=5;c6=6;c7=7;};if (xDir ==-1 ){c0=7;c1=6;c2=5;c3=4;c4=3;c5=2;c6=1;c7=0;};}
if (yDir==-1){if (xDir==1){c0=56;c1=57;c2=58;c3=59;c4=60;c5=61;c6=62;c7=63;};if (xDir ==-1 ){c0=63;c1=62;c2=61;c3=60;c4=59;c5=58;c6=57;c7=56;};};pixfy+=1;
tft.fillRectFast64colors(displayPixelWidth *j,displayPixelHeight* i,displayPixelWidth,displayPixelHeight,//no need to move or divide here. it is done in 64 color at once library
                          pgm_read_word_near(camColors+pixelsubsampledata[c0]),pgm_read_word_near(camColors+pixelsubsampledata[c1]),pgm_read_word_near(camColors+pixelsubsampledata[c2]),pgm_read_word_near(camColors+pixelsubsampledata[c3]),
                          pgm_read_word_near(camColors+pixelsubsampledata[c4]),pgm_read_word_near(camColors+pixelsubsampledata[c5]),pgm_read_word_near(camColors+pixelsubsampledata[c6]),pgm_read_word_near(camColors+pixelsubsampledata[c7]));
                                                //for advances pixel buffer to work we still need to do init and addressing part, this also sends first 8 16 bit colors!
          //we enhance brightness to remove pixle effect  
          byte totalloops=0;
           while (totalloops !=8-1){//we need to run lcd commands and first part, but rest can be tighty looped.
                         if (yDir ==1 ){c0+=8;c1+=8;c2+=8;c3+=8;c4+=8;c5+=8;c6+=8;c7+=8;}else{c0-=8;c1-=8;c2-=8;c3-=8;c4-=8;c5-=8;c6-=8;c7-=8;};
          
              #if spi_buffer_local == false  //we can only spi buffer data after commands sent wich are included in first bacth
          tft.fillRectFast64colors2(pgm_read_word_near(camColors+pixelsubsampledata[c0]),pgm_read_word_near(camColors+pixelsubsampledata[c1]),pgm_read_word_near(camColors+pixelsubsampledata[c2]),pgm_read_word_near(camColors+pixelsubsampledata[c3]),
                                    pgm_read_word_near(camColors+pixelsubsampledata[c4]),pgm_read_word_near(camColors+pixelsubsampledata[c5]),pgm_read_word_near(camColors+pixelsubsampledata[c6]),pgm_read_word_near(camColors+pixelsubsampledata[c7]));

              #else        //sinse we are loading spi buffer here, we need to generate pixels of 2*2. can shrink code later on
                           uint16_t cachcol;//speeds things up a little
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c0]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);//it takes small amount of time to read progmem, but less to 
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c1]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);//read bytes
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c2]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c3]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c4]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);                                                      
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c5]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c6]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c7]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c0]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);//it takes small amount of time to read progmem, but less to 
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c1]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);//read bytes
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c2]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c3]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c4]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);                                                      
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c5]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c6]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c7]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);                          
              #endif  
                             totalloops++; 
                            }
                  
                  //line below needed for if spi buffer mode not used.
                  if (yDir ==1 ){c0+=8;c1+=8;c2+=8;c3+=8;c4+=8;c5+=8;c6+=8;c7+=8;}else{c0-=8;c1-=8;c2-=8;c3-=8;c4-=8;c5-=8;c6-=8;c7-=8;};
        
             #if spi_buffer_local == false   //we can buffer last part of 64 color pixels command because it is built int  SpiFinAndFinalLcdCommandsForWrite();        
            tft.End64colorCommand();//this finishes up
            #else
            //we sovel ending in buffer with SpiFinAndFinalLcdCommandsForWrite(); which is before pixel write. the idea is to do work while preparing pixels.
             #endif            
            


#endif
#endif //interpolate
#if interpolatemode ==4  //this is sub sample processes 128x128
#if spi_buffer_local == true
while(!(SPSR & (1<<SPIF) ));
SpiFinAndFinalLcdCommandsForWrite();//this allows buffer to empty out and all spi data writes to complete, including finilization of lcd commands. so we dont confuse lcd chip
#endif
 
#if subpixelcoloroptimized >0
byte c0,c1,c2,c3,c4,c5,c6,c7;
if (yDir==1){if (xDir ==1 ){c0=0;c1=1;c2=2;c3=3;c4=4;c5=5;c6=6;c7=7;};if (xDir ==-1 ){c0=7;c1=6;c2=5;c3=4;c4=3;c5=2;c6=1;c7=0;};}
if (yDir==-1){if (xDir==1){c0=56;c1=57;c2=58;c3=59;c4=60;c5=61;c6=62;c7=63;};if (xDir ==-1 ){c0=63;c1=62;c2=61;c3=60;c4=59;c5=58;c6=57;c7=56;};};pixfy+=1;
tft.fillRectFast64AveragePixelscolorsStart(displayPixelWidth *j,displayPixelHeight* i,displayPixelWidth,displayPixelHeight,//no need to move or divide here. it is done in 64 color at once library
                          pgm_read_word_near(camColors+pixelsubsampledata[c0]),pgm_read_word_near(camColors+pixelsubsampledata[c1]),pgm_read_word_near(camColors+pixelsubsampledata[c2]),pgm_read_word_near(camColors+pixelsubsampledata[c3]),
                          pgm_read_word_near(camColors+pixelsubsampledata[c4]),pgm_read_word_near(camColors+pixelsubsampledata[c5]),pgm_read_word_near(camColors+pixelsubsampledata[c6]),pgm_read_word_near(camColors+pixelsubsampledata[c7]));
                                                //for advances pixel buffer to work we still need to do init and addressing part, this also sends first 8 16 bit colors!
          //we enhance brightness to remove pixle effect  
          byte totalloops=0;
           while (totalloops !=8-1){//we need to run lcd commands and first part, but rest can be tighty looped.
                         if (yDir ==1 ){c0+=8;c1+=8;c2+=8;c3+=8;c4+=8;c5+=8;c6+=8;c7+=8;}else{c0-=8;c1-=8;c2-=8;c3-=8;c4-=8;c5-=8;c6-=8;c7-=8;};
          
              #if spi_buffer_local == false  //we can only spi buffer data after commands sent wich are included in first bacth
          tft.fillRectFast64AveragePixelscolors(pgm_read_word_near(camColors+pixelsubsampledata[c0]),pgm_read_word_near(camColors+pixelsubsampledata[c1]),pgm_read_word_near(camColors+pixelsubsampledata[c2]),pgm_read_word_near(camColors+pixelsubsampledata[c3]),
                                    pgm_read_word_near(camColors+pixelsubsampledata[c4]),pgm_read_word_near(camColors+pixelsubsampledata[c5]),pgm_read_word_near(camColors+pixelsubsampledata[c6]),pgm_read_word_near(camColors+pixelsubsampledata[c7]));

              #else        //sinse we are loading spi buffer here, we need to generate pixels of 2*2. can shrink code later on
                           uint16_t cachcol;//speeds things up a little
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c0]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);//it takes small amount of time to read progmem, but less to 
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c1]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);//read bytes
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c2]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c3]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c4]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);                                                      
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c5]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c6]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c7]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c0]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);//it takes small amount of time to read progmem, but less to 
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c1]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);//read bytes
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c2]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c3]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c4]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);                                                      
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c5]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c6]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);
                           cachcol= pgm_read_word_near(camColors+pixelsubsampledata[c7]);loadToSpiLocalBuffer(cachcol);loadToSpiLocalBuffer(cachcol);                          
              #endif  
                             totalloops++; 
                            }
                  
                  //line below needed for if spi buffer mode not used.
                  if (yDir ==1 ){c0+=8;c1+=8;c2+=8;c3+=8;c4+=8;c5+=8;c6+=8;c7+=8;}else{c0-=8;c1-=8;c2-=8;c3-=8;c4-=8;c5-=8;c6-=8;c7-=8;};
        
             #if spi_buffer_local == false   //we can buffer last part of 64 color pixels command because it is built int  SpiFinAndFinalLcdCommandsForWrite();        
            tft.End64AveragePixelscolorCommand();//this finishes up
            #else
            //we sovel ending in buffer with SpiFinAndFinalLcdCommandsForWrite(); which is before pixel write. the idea is to do work while preparing pixels.
             #endif            
            


#endif
#endif
long tempt=micros()-timer;
Serial.println(tempt);

//this is end of 32*32 upscale of buffer
//below ends the check buffer loop
#if showcolorbar == true
}
#endif
#if optimize > 0 
}
#endif
#if optimize >1 
    }
#endif
#if noisefilter > 0 & optimize == 2 //optimize needs to be 2 for noise filter to work
  } //we add in case noise filter code in existence
#endif  

      i++;   
//old way
  //         tft.fillRect(displayPixelHeight * (i /8), displayPixelWidth * (i % 8),
    //    displayPixelHeight, displayPixelWidth, camColors[colorIndex]);




  }
//we keep lines below for testing results  
//timer =micros()-timer;
//Serial.println(timer);
//we sample one time per frame 
//4000 is just an upper limit i pulled out of thin air, more color space requires wider range
 if (compressionnumber>speedUpCompression){if (compressionflux<50) {compressionflux+=1;};
 }//we increase range more slowly 
 else{
  if (compressionflux>0){compressionflux-=1;}//else{if (compressionflux>0){compressionflux-=1;}}
 }


//Serial.println(compressionnumber);//for testing
#if interpolatemode > 2 //this for testin 32x32 buffer, to then upscale to 64x64
//this code is for output of buffer to display it tests buffer to make sure it is working correctly. now it is!
//this buffer that it is testing will go away eventually.
/*
for (int jx=0;jx<16;jx++){
for (int ix=0;ix<16;ix++){
//this is where subsample 32x32 code is put



//line below outputs 16x16 data,we want to sub sample that data! this is temp. eventually buffer will not be needed.
fillRectFast(jx*8 , //we reduce pixle size and step over in raster extra pixels created
ix*8,
8,//we divide width of pixels. /2,4,8,16 is fast and compiler can just do bit shift for it
8,//we divide hieght of pixels.
(uint16_t)pgm_read_word_near(camColors+postbuffer[jx*16+ix]));  //we update pixel location with new subsampled pixel.

}
}
*/
/*
//32x32 buffer works test 32x32 buffer just the buffer
for (int jx2=0;jx2<32;jx2++){
for (int ix2=0;ix2<32;ix2++){
//this is where subsample 32x32 code is put
Serial.println(ix2);


//line below outputs 16x16 data,we want to sub sample that data! this is temp. eventually buffer will not be needed.
fillRectFast(jx2*4 , //we reduce pixle size and step over in raster extra pixels created
ix2*4,
4,//we divide width of pixels. /2,4,8,16 is fast and compiler can just do bit shift for it
4,//we divide hieght of pixels.
(uint16_t)pgm_read_word_near(camColors+postbuffer2[jx2*32+ix2]));  //we update pixel location with new subsampled pixel.

}
}
*/
#endif
 
displayaddons();//this is for bar and number showing

}


