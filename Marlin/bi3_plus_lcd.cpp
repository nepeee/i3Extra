#include "temperature.h"
#include "ultralcd.h"
#include "bi3_plus_lcd.h"

#include "Marlin.h"
#include "language.h"
#include "cardreader.h"
#include "temperature.h"
#include "stepper.h"
#include "configuration_store.h"
#include "utility.h"
#include "watchdog.h"

#define OPMODE_NONE 0
#define OPMODE_LEVEL_INIT 1
#define OPMODE_LOAD_FILAMENT 2
#define OPMODE_UNLOAD_FILAMENT 3

uint8_t lcdBuff[26];
uint16_t fileIndex = 0;
millis_t nextOpTime, nextLcdUpdate = 0;
uint8_t opMode = OPMODE_NONE;

//init OK
void lcdSetup() {
  Serial2.begin(250000);

  lcdSendMarlinVersion();
  lcdShowPage(0x01);
}

//lcd status update OK
void lcdTask() {
  readLcdSerial();

  millis_t ms = millis();
  executeLoopedOperation(ms);
  lcdStatusUpdate(ms);
}

void executeLoopedOperation(millis_t ms) {
  if ((opMode != OPMODE_NONE) && (ELAPSED(ms, nextOpTime))) {
    if (opMode == OPMODE_LEVEL_INIT) {
      if (axis_homed[X_AXIS] && axis_homed[Y_AXIS] && axis_homed[Z_AXIS]) {//stuck if levelling problem?
        opMode = OPMODE_NONE;
        lcdShowPage(56);//level 2 menu
      }
      else
        nextOpTime = ms + 200;
    }
    else if (opMode == OPMODE_UNLOAD_FILAMENT) {
      if (thermalManager.current_temperature[0] >= 190)
        enqueue_and_echo_commands_P(PSTR("G1 E-0.5 F60"));
      nextOpTime = ms + 500;
    }
    else if (opMode == OPMODE_LOAD_FILAMENT) {
      if (thermalManager.current_temperature[0] >= 190)
        enqueue_and_echo_commands_P(PSTR("G1 E0.5 F60"));
      nextOpTime = ms + 500;
    }
  }
}

void lcdStatusUpdate(millis_t ms) {
  if (ELAPSED(ms, nextLcdUpdate)) {
    nextLcdUpdate = ms + 500;

    lcdBuff[0] = 0x5A;
    lcdBuff[1] = 0xA5;
    lcdBuff[2] = 0x0F; //data length
    lcdBuff[3] = 0x82; //write data to sram
    lcdBuff[4] = 0x00; //starting at 0 vp
    lcdBuff[5] = 0x00;

    int tmp = thermalManager.target_temperature[0];
    lcdBuff[6] = highByte(tmp);//0x00 target extruder temp
    lcdBuff[7] = lowByte(tmp);

    tmp = thermalManager.degHotend(0);
    lcdBuff[8] = highByte(tmp);//0x01 extruder temp
    lcdBuff[9] = lowByte(tmp);

    lcdBuff[10] = 0x00;//0x02 target bed temp
    lcdBuff[11] = thermalManager.target_temperature_bed;

    lcdBuff[12] = 0x00;//0x03 bed temp
    lcdBuff[13] = thermalManager.degBed();

    lcdBuff[14] = 0x00; //0x04 fan speed
    lcdBuff[15] = (int16_t)fanSpeeds[0] * 100 / 255;

    lcdBuff[16] = 0x00;//0x05 card progress
    lcdBuff[17] = card.percentDone();

    Serial2.write(lcdBuff, 18);
  }
}

//show page OK
void lcdShowPage(uint8_t pageNumber) {
  lcdBuff[0] = 0x5A;//frame header
  lcdBuff[1] = 0xA5;

  lcdBuff[2] = 0x04;//data length

  lcdBuff[3] = 0x80;//command - write data to register
  lcdBuff[4] = 0x03;

  lcdBuff[5] = 0x00;
  lcdBuff[6] = pageNumber;

  Serial2.write(lcdBuff, 7);
}

//receive data from lcd OK
void readLcdSerial() {
  if (Serial2.available() > 0) {
    if (Serial2.read() != 0x5A)
      return;

    while (Serial2.available() < 1);

    if (Serial2.read() != 0xA5)
      return;

    while (Serial2.available() < 3);

    Serial2.read();//data length
    Serial2.read();//command

    if (Serial2.read() != 4) //VP MSB
      return;

    while (Serial2.available() < 4);

    uint8_t lcdCommand = Serial2.read(); // VP LSB
    Serial2.read();// LEN ?
    Serial2.read(); //KEY VALUE MSB
    uint8_t lcdData = Serial2.read(); //KEY VALUE LSB

    switch (lcdCommand) {
      case 0x32: {//SD list navigation up/down OK
          if (card.sdprinting)
            lcdShowPage(33); //show print menu
          else {
            if (lcdData == 0) {
              card.initsd();
              if (card.cardOK)
                fileIndex = card.getnrfilenames() - 1;
            }

            if (card.cardOK) {
              uint16_t fileCnt = card.getnrfilenames();
              card.getWorkDirName();//??

              if (fileCnt > 6) {
                if (lcdData == 1) { //UP
                  if ((fileIndex + 6) < fileCnt)
                    fileIndex += 6;
                }
                else if (lcdData == 2) { //DOWN
                  if (fileIndex >= 6)
                    fileIndex -= 6;
                }
              }

              lcdBuff[0] = 0x5A;
              lcdBuff[1] = 0xA5;
              lcdBuff[2] = 0x9F;
              lcdBuff[3] = 0x82;
              lcdBuff[4] = 0x01;
              lcdBuff[5] = 0x00;
              Serial2.write(lcdBuff, 6);

              for (uint8_t i = 0; i < 6; i++) {
                card.getfilename(fileIndex - i);
                strncpy(lcdBuff, card.longFilename, 26);
                Serial2.write(lcdBuff, 26);
              }

              if (lcdData == 0)
                lcdShowPage(31); //show sd card menu
            }
          }
          break;
        }
      case 0x33: {//FILE SELECT OK
          if (card.cardOK) {
            card.getfilename(fileIndex - lcdData);
            
            lcdBuff[0] = 0x5A;
            lcdBuff[1] = 0xA5;
            lcdBuff[2] = 0x1D;
            lcdBuff[3] = 0x82;
            lcdBuff[4] = 0x01;
            lcdBuff[5] = 0x4E;
            Serial2.write(lcdBuff, 6);
            
            strncpy(lcdBuff, card.longFilename, 26);
            Serial2.write(lcdBuff, 26);

            card.openFile(card.filename, true);
            card.startFileprint();

            lcdShowPage(33);//print menu
          }
          break;
        }
      case 0x35: {//print stop OK
          card.stopSDPrint();
          clear_command_queue();
          quickstop_stepper();
          thermalManager.disable_all_heaters();
          fanSpeeds[0] = 0;

          lcdShowPage(29); //main menu
          break;
        }
      case 0x36: {//print pause OK
          card.pauseSDPrint();
          enqueue_and_echo_commands_P(PSTR("G91"));
          enqueue_and_echo_commands_P(PSTR("G1 Z10 F3000"));
          enqueue_and_echo_commands_P(PSTR("G90"));
          break;
        }
      case 0x37: {//print start OK
          //homeing ?
          card.startFileprint();
          break;
        }
      case 0x3C: {//preheat pla OK
          thermalManager.setTargetHotend(185, 0);
          thermalManager.setTargetBed(50);
          break;
        }
      case 0x3D: {//preheat abs OK
          thermalManager.setTargetHotend(210, 0);
          thermalManager.setTargetBed(70);
          break;
        }
      case 0x34: {//cool down OK
          thermalManager.disable_all_heaters();
          break;
        }
      case 0x3E: {//send pid/motor config to lcd OK
          lcdBuff[0] = 0x5A;
          lcdBuff[1] = 0xA5;

          lcdBuff[2] = 0x11;

          lcdBuff[3] = 0x82;

          lcdBuff[4] = 0x03;
          lcdBuff[5] = 0x24;

          uint16_t tmp = planner.axis_steps_per_mm[X_AXIS] * 10;
          lcdBuff[6] = highByte(tmp);
          lcdBuff[7] = lowByte(tmp);

          tmp = planner.axis_steps_per_mm[Y_AXIS] * 10;
          lcdBuff[8] = highByte(tmp);
          lcdBuff[9] = lowByte(tmp);

          tmp = planner.axis_steps_per_mm[Z_AXIS] * 10;
          lcdBuff[10] = highByte(tmp);
          lcdBuff[11] = lowByte(tmp);

          tmp = planner.axis_steps_per_mm[E_AXIS] * 10;
          lcdBuff[12] = highByte(tmp);
          lcdBuff[13] = lowByte(tmp);

          tmp = PID_PARAM(Kp, 0) * 10;
          lcdBuff[14] = highByte(tmp);
          lcdBuff[15] = lowByte(tmp);

          tmp = unscalePID_i(PID_PARAM(Ki, 0)) * 10;
          lcdBuff[16] = highByte(tmp);
          lcdBuff[17] = lowByte(tmp);

          tmp = unscalePID_d(PID_PARAM(Kd, 0)) * 10;
          lcdBuff[18] = highByte(tmp);
          lcdBuff[19] = lowByte(tmp);

          Serial2.write(lcdBuff, 20);

          if (lcdData)
            lcdShowPage(45);//show pid screen
          else
            lcdShowPage(47);//show motor screen
          break;
        }
      case 0x3F: {//save pid/motor config OK
          lcdBuff[0] = 0x5A;
          lcdBuff[1] = 0xA5;

          lcdBuff[2] = 0x04;

          lcdBuff[3] = 0x83;

          lcdBuff[4] = 0x03;
          lcdBuff[5] = 0x24;

          lcdBuff[6] = 0x07;

          Serial2.write(lcdBuff, 7);

          uint8_t bytesRead = Serial2.readBytes(lcdBuff, 21);
          if ((bytesRead == 21) && (lcdBuff[0] == 0x5A) && (lcdBuff[1] == 0xA5)) {
            planner.axis_steps_per_mm[X_AXIS] = (float)((uint16_t)lcdBuff[7] * 255 + lcdBuff[8]) / 10;
            Serial.println(lcdBuff[7]);
            Serial.println(lcdBuff[8]);
            Serial.println(lcdBuff[9]);
            Serial.println(lcdBuff[10]);
            planner.axis_steps_per_mm[Y_AXIS] = (float)((uint16_t)lcdBuff[9] * 255 + lcdBuff[10]) / 10;
            planner.axis_steps_per_mm[Z_AXIS] = (float)((uint16_t)lcdBuff[11] * 255 + lcdBuff[12]) / 10;
            planner.axis_steps_per_mm[E_AXIS] = (float)((uint16_t)lcdBuff[13] * 255 + lcdBuff[14]) / 10;

            PID_PARAM(Kp, 0) = (float)((uint16_t)lcdBuff[15] * 255 + lcdBuff[16]) / 10;
            PID_PARAM(Ki, 0) = scalePID_i((float)((uint16_t)lcdBuff[17] * 255 + lcdBuff[18]) / 10);
            PID_PARAM(Kd, 0) = scalePID_d((float)((uint16_t)lcdBuff[19] * 255 + lcdBuff[20]) / 10);

            settings.save();
            lcdShowPage(43);//show system menu
          }
          break;
        }
      case 0x42: {//factory reset OK
          settings.reset();
          break;
        }
      case 0x47: {//print config open OK
          lcdBuff[0] = 0x5A;
          lcdBuff[1] = 0xA5;

          lcdBuff[2] = 0x0B;

          lcdBuff[3] = 0x82;

          lcdBuff[4] = 0x03;
          lcdBuff[5] = 0x2B;

          lcdBuff[6] = highByte(flow_percentage[0]); //0x2B
          lcdBuff[7] = lowByte(flow_percentage[0]);

          int temp = thermalManager.degTargetHotend(0);
          lcdBuff[8] = highByte(temp); //0x2C
          lcdBuff[9] = lowByte(temp);

          lcdBuff[10] = 0x00;//0x2D
          lcdBuff[11] = (int)thermalManager.degTargetBed();

          lcdBuff[12] = 0x00;//0x2E
          lcdBuff[13] = (uint16_t)fanSpeeds[0] * 100 / 255;

          Serial2.write(lcdBuff, 14);

          lcdShowPage(35);//print config
          break;
        }
      case 0x40: {//print config save OK
          lcdBuff[0] = 0x5A;
          lcdBuff[1] = 0xA5;

          lcdBuff[2] = 0x04;//4 byte

          lcdBuff[3] = 0x83;//command

          lcdBuff[4] = 0x03;// start addr
          lcdBuff[5] = 0x2B;

          lcdBuff[6] = 0x04; //4 vp

          Serial2.write(lcdBuff, 7);

          uint8_t bytesRead = Serial2.readBytes(lcdBuff, 15);
          if ((bytesRead == 15) && (lcdBuff[0] == 0x5A) && (lcdBuff[1] == 0xA5)) {
            flow_percentage[0] = (uint16_t)lcdBuff[7] * 255 + lcdBuff[8];
            thermalManager.setTargetHotend((uint16_t)lcdBuff[9] * 255 + lcdBuff[10], 0);
            thermalManager.setTargetBed(lcdBuff[12]);
            fanSpeeds[0] = (uint16_t)lcdBuff[14] * 255 / 100;
            lcdShowPage(33);// show print menu
          }
          break;
        }
      case 0x48: {//load filament OK
          thermalManager.setTargetHotend(200, 0);
          enqueue_and_echo_commands_P(PSTR("G91")); // relative mode
          nextOpTime = millis() + 500;
          opMode = OPMODE_LOAD_FILAMENT;
          break;
        }
      case 0x49: {//unload filament OK
          thermalManager.setTargetHotend(200, 0);
          enqueue_and_echo_commands_P(PSTR("G91")); // relative mode
          nextOpTime = millis() + 500;
          opMode = OPMODE_UNLOAD_FILAMENT;
          break;
        }
      case 0x4A: {//load/unload filament back OK
          opMode = OPMODE_NONE;
          clear_command_queue();
          enqueue_and_echo_commands_P(PSTR("G90")); // absolute mode
          thermalManager.setTargetHotend(0, 0);
          lcdShowPage(49);//filament menu
          break;
        }
      case 0x4C: {//level menu OK
          if (lcdData == 0) {
            lcdShowPage(55); //level 1
            axis_homed[X_AXIS] = axis_homed[Y_AXIS] = axis_homed[Z_AXIS] = false;
            enqueue_and_echo_commands_P(PSTR("G90")); //absolute mode
            enqueue_and_echo_commands_P((PSTR("G28")));//homeing
            nextOpTime = millis() + 200;
            opMode = OPMODE_LEVEL_INIT;
          }
          else if (lcdData == 1) { //fl
            enqueue_and_echo_commands_P((PSTR("G1 X30 Y30 Z10 F6000")));
            enqueue_and_echo_commands_P((PSTR("G28 Z0")));
          }
          else if (lcdData == 2) { //rr
            enqueue_and_echo_commands_P((PSTR("G1 X170 Y170 Z10 F6000")));
            enqueue_and_echo_commands_P((PSTR("G28 Z0")));
          }
          else if (lcdData == 3) { //fr
            enqueue_and_echo_commands_P((PSTR("G1 X170 Y30 Z10 F6000")));
            enqueue_and_echo_commands_P((PSTR("G28 Z0")));
          }
          else if (lcdData == 4) { //rl
            enqueue_and_echo_commands_P((PSTR("G1 X30 Y170 Z10 F6000")));
            enqueue_and_echo_commands_P((PSTR("G28 Z0")));
          }
          else if (lcdData == 5) { //c
            enqueue_and_echo_commands_P((PSTR("G1 X100 Y100 Z10 F6000")));
            enqueue_and_echo_commands_P((PSTR("G28 Z0")));
          }
          else if (lcdData == 6) { //back
            enqueue_and_echo_commands_P((PSTR("G1 Z30 F6000")));
            lcdShowPage(37); //tool menu
          }
          break;
        }
      case 0x01: {//move OK!!!
          enqueue_and_echo_commands_P(PSTR("G91")); // relative mode

          //if (manual_move_axis != (int8_t)NO_AXIS && ELAPSED(millis(), manual_move_start_time) && !planner.is_full()) {
          //  planner.buffer_line_kinematic(current_position, MMM_TO_MMS(manual_feedrate_mm_m[manual_move_axis]), 0);

          if (lcdData < 2) {
            if (!axis_homed[X_AXIS])
              enqueue_and_echo_commands_P(PSTR("G28 X0"));
            
            if (lcdData == 0)
              enqueue_and_echo_commands_P(PSTR("G1 X10 F3000"));
            else if (lcdData == 1)
              enqueue_and_echo_commands_P(PSTR("G1 X-10 F3000"));
          }
          else if (lcdData < 4) {
            if (!axis_homed[Y_AXIS])
              enqueue_and_echo_commands_P(PSTR("G28 Y0"));

            if (lcdData == 2)
              enqueue_and_echo_commands_P(PSTR("G1 Y10 F3000"));
            else if (lcdData == 3)
              enqueue_and_echo_commands_P(PSTR("G1 Y-10 F3000"));
          }
          else if (lcdData < 6) {
            if (!axis_homed[Z_AXIS])
              enqueue_and_echo_commands_P(PSTR("G28 Z0"));

            if (lcdData == 4)
              enqueue_and_echo_commands_P(PSTR("G1 Z10 F3000"));
            else if (lcdData == 5)
              enqueue_and_echo_commands_P(PSTR("G1 Z-10 F3000"));
          }
          else if (thermalManager.degHotend(0) >= 180) {
            if (lcdData == 6)
              enqueue_and_echo_commands_P(PSTR("G1 E10 F60"));
            else if (lcdData == 7)
              enqueue_and_echo_commands_P(PSTR("G1 E-10 F60"));
          }

          enqueue_and_echo_commands_P(PSTR("G90")); // absolute mode
          break;
        }
      case 0x54: {//disable motors OK!!!
          enqueue_and_echo_commands_P(PSTR("M84"));
          axis_homed[X_AXIS] = axis_homed[Y_AXIS] = axis_homed[Z_AXIS] = false;
          break;
        }
      case 0x43: {//home x OK!!!
          enqueue_and_echo_commands_P(PSTR("G28 X0"));
          break;
        }
      case 0x44: {//home y OK!!!
          enqueue_and_echo_commands_P(PSTR("G28 Y0"));
          break;
        }
      case 0x45: {//home z OK!!!
          enqueue_and_echo_commands_P(PSTR("G28 Z0"));
          break;
        }
      case 0x1C: {//home xyz OK!!!
          enqueue_and_echo_commands_P(PSTR("G28"));
          break;
        }
      case 0xFF: {
          lcdShowPage(58); //update screen
          while (1) {
            watchdog_reset();
            if (Serial.available())
              Serial2.write(Serial.read());
            if (Serial2.available())
              Serial.write(Serial2.read());
          }
          break;
        }
      default:
        break;
    }
  }
}

void lcdSendMarlinVersion() {
  lcdBuff[0] = 0x5A;
  lcdBuff[1] = 0xA5;
  lcdBuff[2] = 0x12; //data length
  lcdBuff[3] = 0x82; //write data to sram
  lcdBuff[4] = 0x05; //starting at 0x0500 vp
  lcdBuff[5] = 0x00;

  strncpy((char*)lcdBuff + 6, SHORT_BUILD_VERSION, 15);
  Serial2.write(lcdBuff, 21);
}
