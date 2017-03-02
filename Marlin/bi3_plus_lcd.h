#ifndef BI3PLUSLCD_H
#define BI3PLUSLCD_H

#include "Marlin.h"

void lcdSetup();
void lcdTask();

void executeLoopedOperation(millis_t ms);
void lcdStatusUpdate(millis_t ms);
void lcdShowPage(uint8_t pageNumber);
void readLcdSerial();
void lcdSendMarlinVersion();

#endif
