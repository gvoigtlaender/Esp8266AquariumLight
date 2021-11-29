#ifndef SRC_CONFIG_H_
#define SRC_CONFIG_H_

/* Copyright 2018 Georg Voigtlaender gvoigtlaender@googlemail.com */
const char VERSION_STRING[] = "0.0.21.1";
const char APPNAME[] = "ESP Aqua Light";
const char SHORTNAME[] = "ESPAL";

#define USE_DISPLAY 0
// #define USE_ASYNCTCP 0

#define PIN_BLUE D7
#define PIN_WHITE D0
#define PWM_RANGE 1023
#define PIN_BTN D2
#define PIN_LED D6

#endif // SRC_CONFIG_H_
