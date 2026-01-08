/*******************************************************
 * Camera_2_Lcd.ino - Camera to TFT LCD
 *
 * Author: Kevin Chen
 * Date: 2023/11/16
 *
 * Version: 1.0.0
 *
 * This code was written by Kevin Chen.
 *
 * This is an open-source program, and it can be freely used, modified, and distributed under the following conditions:
 *
 * 1. The original copyright notice must not be removed or modified.
 * 2. Projects or products using this code must acknowledge the original author's name and the source in applicable documentation, websites, or other related materials.
 * 3. Any derivative work based on this code must state its origin and retain the original copyright notice in its documentation.
 * 4. This code must not be used for any activities that may infringe upon the rights of others, be unlawful, or harmful, whether in a commercial or non-commercial environment.
 *
 * This code is provided "as is" with no warranty, expressed or implied. The author is not liable for any losses or damages caused by using the code.
 *
 * Users of Camera_2_Lcd.ino assume all risks associated with its use, and the author shall not be held responsible for any consequences.
 *
 * For more information about our company, please visit: www.makdev.net
 *
 *  1.Based on AmebaILI9341.h, implement the functionality to display Camera output on TFT LCD and utilize the SD Card feature to achieve basic photo capture.
 *
 *  2.Before starting the project, please install the TJpg_Decoder library. In the library's configuration file, User_Config.h, comment out line 5 which reads: #define TJPGD_LOAD_SD_LIBRARY
 *******************************************************/

#include "VideoStream.h"
#include "SPI.h"
#include "AmebaILI9341.h"
#include "TJpg_Decoder.h"
#include "AmebaFatFS.h"

#define CHANNEL 0

#define TFT_RESET 5
#define TFT_DC    4
#define TFT_CS    SPI_SS
#define FILENAME "ximg_"

AmebaILI9341 tft = AmebaILI9341(TFT_CS, TFT_DC, TFT_RESET);
#define ILI9341_SPI_FREQUENCY 50000000

// Always run at 1080p (FHD) so saved photos are 1920x1080.
// Preview will be downscaled on the fly.
VideoSetting config(VIDEO_FHD, CAM_FPS, VIDEO_JPEG, 1);

uint32_t img_addr = 0;
uint32_t img_len  = 0;

AmebaFatFS fs;
int button = 17;
volatile int button_State = 0;
bool Camer_cap;
uint32_t count = 0;

unsigned long boot_ms = 0;
int flash_pending = 0;

void button_Handler(uint32_t id, uint32_t event) {
  if (button_State == 0) button_State = 1;
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  tft.drawBitmap(x, y, w, h, bitmap);
  return 1;
}

void setup() {
  Serial.begin(115200);
  Serial.println("TFT ILI9341 ");

  SPI.setDefaultFrequency(ILI9341_SPI_FREQUENCY);

  pinMode(button, INPUT_PULLUP);
  digitalSetIrqHandler(button, button_Handler);
  pinMode(button, INPUT_IRQ_FALL);

  config.setRotation(2);
  Camera.configVideoChannel(CHANNEL, config);
  Camera.videoInit();
  Camera.channelBegin(CHANNEL);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);

  TJpgDec.setJpgScale(4);            // 1920x1080 -> 480x270
  TJpgDec.setCallback(tft_output);

  // ---- PERSIST COUNT: load last value from SD ----
  if (fs.begin()) {
    String idxPath = String(fs.getRootPath()) + "ximg.idx";
    File idx = fs.open(idxPath);         // open if exists
    if (idx) {
      String s = idx.readString();
      count = s.toInt();                 // resume from last
      idx.close();
    } else {
      count = 0;                         // first run
    }
    fs.end();
  }
  // ------------------------------------------------

  boot_ms = millis();
}

void loop() {
  // live FHD frame for preview + save
  Camera.getImage(CHANNEL, &img_addr, &img_len);

  // save on shutter
  if (button_State == 1 && (millis() - boot_ms > 1000) && img_len > 0) {
    if (fs.begin()) {
      String imgPath = String(fs.getRootPath()) + String(FILENAME) + String(count) + String(".jpg");
      File file = fs.open(imgPath);
      if (file) {
        file.write((uint8_t *)img_addr, img_len);   // saves full 1920x1080 JPEG
        file.close();
        count++;
        flash_pending = 1;

        // ---- PERSIST COUNT: store updated value ----
        String idxPath = String(fs.getRootPath()) + "ximg.idx";
        File idx = fs.open(idxPath);                // create/overwrite
        if (idx) { idx.print(count); idx.close(); }
        // --------------------------------------------
      }
      fs.end();
    }
    button_State = 0;
  }

  // simple flash after save
  if (flash_pending) {
    tft.fillScreen(ILI9341_WHITE);
    delay(60);
    tft.fillScreen(ILI9341_BLACK);
    flash_pending = 0;
  }

  // draw centered FHD preview scaled to 480x270
  uint16_t w, h;
  TJpgDec.getJpgSize(&w, &h, (uint8_t *)img_addr, img_len);  // expect 1920x1080
  w >>= 2;   // 480
  h >>= 2;   // 270
  int x = (tft.getWidth()  - w) / 2;   // 0
  int y = (tft.getHeight() - h) / 2;   // 25
  TJpgDec.drawJpg(x, y, (uint8_t *)img_addr, img_len);
}

