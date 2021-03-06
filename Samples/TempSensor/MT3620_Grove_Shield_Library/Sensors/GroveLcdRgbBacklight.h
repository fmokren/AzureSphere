//GROVE_NAME        "Grove -  LCD RGB Backlight"
//SKU               104030001
//WIKI_URL          https://www.seeedstudio.com/Grove-LCD-RGB-Backlight-p-1643.html

#pragma once
void* GroveLcdRgbBacklight_Open(int i2cFd);

void GroveLcdRgbBacklight_ClearDisplay(void *this);

void GroveLcdRgbBacklight_SetBacklightRgb(void *this, uint8_t red, uint8_t green, uint8_t blue);