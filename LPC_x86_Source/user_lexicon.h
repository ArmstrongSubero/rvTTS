/*
 * user_lexicon.h
 *
 * Words CMUdict does not carry: SI prefix compounds, component names,
 * bus acronyms. Without an entry they fall to reciter_guess, which
 * returns no stress. Acronyms are spelled the way they are said.
 */

#ifndef USER_LEXICON_H
#define USER_LEXICON_H

#include <stdint.h>
#include "phonemes.h"

static const uint8_t ulx_ph_AMPS[] = { PH_AE, PH_M, PH_P, PH_S };
static const uint8_t ulx_st_AMPS[] = { 1, 0, 0, 0 };
static const uint8_t ulx_ph_ANTICLOCKWISE[] = { PH_AE, PH_N, PH_T, PH_IY, PH_K, PH_L, PH_AA, PH_K, PH_W, PH_AY, PH_Z };
static const uint8_t ulx_st_ANTICLOCKWISE[] = { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 };
static const uint8_t ulx_ph_BAOCHIP[] = { PH_B, PH_AW, PH_CH, PH_IH, PH_P };
static const uint8_t ulx_st_BAOCHIP[] = { 0, 1, 0, 0, 0 };
static const uint8_t ulx_ph_BOOTLOADER[] = { PH_B, PH_UW, PH_T, PH_L, PH_OW, PH_D, PH_ER };
static const uint8_t ulx_st_BOOTLOADER[] = { 0, 1, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_CALIBRATING[] = { PH_K, PH_AE, PH_L, PH_AH, PH_B, PH_R, PH_EY, PH_T, PH_IH, PH_NG };
static const uint8_t ulx_st_CALIBRATING[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_CHECKSUM[] = { PH_CH, PH_EH, PH_K, PH_S, PH_AH, PH_M };
static const uint8_t ulx_st_CHECKSUM[] = { 0, 1, 0, 0, 0, 0 };
static const uint8_t ulx_ph_COULOMB[] = { PH_K, PH_UW, PH_L, PH_AA, PH_M };
static const uint8_t ulx_st_COULOMB[] = { 0, 1, 0, 0, 0 };
static const uint8_t ulx_ph_DABAO[] = { PH_D, PH_AA, PH_B, PH_AW };
static const uint8_t ulx_st_DABAO[] = { 0, 1, 0, 0 };
static const uint8_t ulx_ph_EEPROM[] = { PH_IY, PH_IY, PH_P, PH_R, PH_AA, PH_M };
static const uint8_t ulx_st_EEPROM[] = { 1, 1, 0, 0, 1, 0 };
static const uint8_t ulx_ph_ENCODER[] = { PH_EH, PH_N, PH_K, PH_OW, PH_D, PH_ER };
static const uint8_t ulx_st_ENCODER[] = { 0, 0, 0, 1, 0, 0 };
static const uint8_t ulx_ph_FARAD[] = { PH_F, PH_AE, PH_R, PH_AH, PH_D };
static const uint8_t ulx_st_FARAD[] = { 0, 1, 0, 0, 0 };
static const uint8_t ulx_ph_GIGAHERTZ[] = { PH_G, PH_IH, PH_G, PH_AH, PH_HH, PH_ER, PH_T, PH_S };
static const uint8_t ulx_st_GIGAHERTZ[] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_GPIO[] = { PH_JH, PH_IY, PH_P, PH_IY, PH_AY, PH_OW };
static const uint8_t ulx_st_GPIO[] = { 0, 1, 0, 1, 1, 1 };
static const uint8_t ulx_ph_GUVARI[] = { PH_G, PH_UW, PH_V, PH_AA, PH_R, PH_IY };
static const uint8_t ulx_st_GUVARI[] = { 0, 0, 0, 1, 0, 0 };
static const uint8_t ulx_ph_HALFWORD[] = { PH_HH, PH_AE, PH_F, PH_W, PH_ER, PH_D };
static const uint8_t ulx_st_HALFWORD[] = { 0, 1, 0, 0, 0, 0 };
static const uint8_t ulx_ph_HDMI[] = { PH_EY, PH_CH, PH_D, PH_IY, PH_EH, PH_M, PH_AY };
static const uint8_t ulx_st_HDMI[] = { 1, 0, 0, 1, 1, 0, 1 };
static const uint8_t ulx_ph_INDUCTORS[] = { PH_IH, PH_N, PH_D, PH_AH, PH_K, PH_T, PH_ER, PH_Z };
static const uint8_t ulx_st_INDUCTORS[] = { 0, 0, 0, 1, 0, 0, 0, 0 };
static const uint8_t ulx_ph_JTAG[] = { PH_JH, PH_EY, PH_T, PH_AE, PH_G };
static const uint8_t ulx_st_JTAG[] = { 0, 1, 0, 1, 0 };
static const uint8_t ulx_ph_KILOHERTZ[] = { PH_K, PH_IH, PH_L, PH_OW, PH_HH, PH_ER, PH_T, PH_S };
static const uint8_t ulx_st_KILOHERTZ[] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_KILOHM[] = { PH_K, PH_IH, PH_L, PH_OW, PH_M };
static const uint8_t ulx_st_KILOHM[] = { 0, 1, 0, 0, 0 };
static const uint8_t ulx_ph_KILOVOLT[] = { PH_K, PH_IH, PH_L, PH_OW, PH_V, PH_OW, PH_L, PH_T };
static const uint8_t ulx_st_KILOVOLT[] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_KILOVOLTS[] = { PH_K, PH_IH, PH_L, PH_OW, PH_V, PH_OW, PH_L, PH_T, PH_S };
static const uint8_t ulx_st_KILOVOLTS[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_LEDS[] = { PH_EH, PH_L, PH_IY, PH_D, PH_IY, PH_Z };
static const uint8_t ulx_st_LEDS[] = { 1, 0, 1, 0, 1, 0 };
static const uint8_t ulx_ph_LITRE[] = { PH_L, PH_IY, PH_T, PH_ER };
static const uint8_t ulx_st_LITRE[] = { 0, 1, 0, 0 };
static const uint8_t ulx_ph_LITRES[] = { PH_L, PH_IY, PH_T, PH_ER, PH_Z };
static const uint8_t ulx_st_LITRES[] = { 0, 1, 0, 0, 0 };
static const uint8_t ulx_ph_MEGOHM[] = { PH_M, PH_EH, PH_G, PH_OW, PH_M };
static const uint8_t ulx_st_MEGOHM[] = { 0, 1, 0, 0, 0 };
static const uint8_t ulx_ph_MICROAMP[] = { PH_M, PH_AY, PH_K, PH_R, PH_OW, PH_AE, PH_M, PH_P };
static const uint8_t ulx_st_MICROAMP[] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MICROCONTROLLER[] = { PH_M, PH_AY, PH_K, PH_R, PH_OW, PH_K, PH_AH, PH_N, PH_T, PH_R, PH_OW, PH_L, PH_ER };
static const uint8_t ulx_st_MICROCONTROLLER[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 };
static const uint8_t ulx_ph_MICROFARAD[] = { PH_M, PH_AY, PH_K, PH_R, PH_OW, PH_F, PH_AE, PH_R, PH_AH, PH_D };
static const uint8_t ulx_st_MICROFARAD[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MICROHENRY[] = { PH_M, PH_AY, PH_K, PH_R, PH_OW, PH_HH, PH_EH, PH_N, PH_R, PH_IY };
static const uint8_t ulx_st_MICROHENRY[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MICROSECOND[] = { PH_M, PH_AY, PH_K, PH_R, PH_OW, PH_S, PH_EH, PH_K, PH_AH, PH_N, PH_D };
static const uint8_t ulx_st_MICROSECOND[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MICROSECONDS[] = { PH_M, PH_AY, PH_K, PH_R, PH_OW, PH_S, PH_EH, PH_K, PH_AH, PH_N, PH_D, PH_Z };
static const uint8_t ulx_st_MICROSECONDS[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MICROVOLT[] = { PH_M, PH_AY, PH_K, PH_R, PH_OW, PH_V, PH_OW, PH_L, PH_T };
static const uint8_t ulx_st_MICROVOLT[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MICROWATT[] = { PH_M, PH_AY, PH_K, PH_R, PH_OW, PH_W, PH_AA, PH_T };
static const uint8_t ulx_st_MICROWATT[] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MILLIAMP[] = { PH_M, PH_IH, PH_L, PH_IY, PH_AE, PH_M, PH_P };
static const uint8_t ulx_st_MILLIAMP[] = { 0, 1, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MILLIAMPS[] = { PH_M, PH_IH, PH_L, PH_IY, PH_AE, PH_M, PH_P, PH_S };
static const uint8_t ulx_st_MILLIAMPS[] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MILLIHENRY[] = { PH_M, PH_IH, PH_L, PH_IY, PH_HH, PH_EH, PH_N, PH_R, PH_IY };
static const uint8_t ulx_st_MILLIHENRY[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MILLIOHM[] = { PH_M, PH_IH, PH_L, PH_IY, PH_OW, PH_M };
static const uint8_t ulx_st_MILLIOHM[] = { 0, 1, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MILLIVOLT[] = { PH_M, PH_IH, PH_L, PH_IY, PH_V, PH_OW, PH_L, PH_T };
static const uint8_t ulx_st_MILLIVOLT[] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MILLIVOLTS[] = { PH_M, PH_IH, PH_L, PH_IY, PH_V, PH_OW, PH_L, PH_T, PH_S };
static const uint8_t ulx_st_MILLIVOLTS[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MILLIWATT[] = { PH_M, PH_IH, PH_L, PH_IY, PH_W, PH_AA, PH_T };
static const uint8_t ulx_st_MILLIWATT[] = { 0, 1, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_MOSFET[] = { PH_M, PH_AA, PH_S, PH_F, PH_EH, PH_T };
static const uint8_t ulx_st_MOSFET[] = { 0, 1, 0, 0, 0, 0 };
static const uint8_t ulx_ph_NANOFARAD[] = { PH_N, PH_AE, PH_N, PH_OW, PH_F, PH_AE, PH_R, PH_AH, PH_D };
static const uint8_t ulx_st_NANOFARAD[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_PHOTODIODE[] = { PH_F, PH_OW, PH_T, PH_OW, PH_D, PH_AY, PH_OW, PH_D };
static const uint8_t ulx_st_PHOTODIODE[] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_PHOTOTRANSISTOR[] = { PH_F, PH_OW, PH_T, PH_OW, PH_T, PH_R, PH_AE, PH_N, PH_Z, PH_IH, PH_S, PH_T, PH_ER };
static const uint8_t ulx_st_PHOTOTRANSISTOR[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 };
static const uint8_t ulx_ph_PICOFARAD[] = { PH_P, PH_IY, PH_K, PH_OW, PH_F, PH_AE, PH_R, PH_AH, PH_D };
static const uint8_t ulx_st_PICOFARAD[] = { 0, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_PIEZO[] = { PH_P, PH_IY, PH_EY, PH_Z, PH_OW };
static const uint8_t ulx_st_PIEZO[] = { 0, 0, 1, 0, 0 };
static const uint8_t ulx_ph_POTENTIOMETER[] = { PH_P, PH_AH, PH_T, PH_EH, PH_N, PH_SH, PH_IY, PH_AA, PH_M, PH_AH, PH_T, PH_ER };
static const uint8_t ulx_st_POTENTIOMETER[] = { 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0 };
static const uint8_t ulx_ph_QSPI[] = { PH_K, PH_Y, PH_UW, PH_EH, PH_S, PH_P, PH_IY, PH_AY };
static const uint8_t ulx_st_QSPI[] = { 0, 0, 1, 1, 0, 0, 1, 1 };
static const uint8_t ulx_ph_RESONATOR[] = { PH_R, PH_EH, PH_Z, PH_AH, PH_N, PH_EY, PH_T, PH_ER };
static const uint8_t ulx_st_RESONATOR[] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_RHEOSTAT[] = { PH_R, PH_IY, PH_AH, PH_S, PH_T, PH_AE, PH_T };
static const uint8_t ulx_st_RHEOSTAT[] = { 0, 1, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_ROVARI[] = { PH_R, PH_OW, PH_V, PH_AA, PH_R, PH_IY };
static const uint8_t ulx_st_ROVARI[] = { 0, 0, 0, 1, 0, 0 };
static const uint8_t ulx_ph_SD[] = { PH_EH, PH_S, PH_D, PH_IY };
static const uint8_t ulx_st_SD[] = { 1, 0, 0, 1 };
static const uint8_t ulx_ph_SERVOS[] = { PH_S, PH_ER, PH_V, PH_OW, PH_Z };
static const uint8_t ulx_st_SERVOS[] = { 0, 1, 0, 0, 0 };
static const uint8_t ulx_ph_SOLENOID[] = { PH_S, PH_OW, PH_L, PH_AH, PH_N, PH_OY, PH_D };
static const uint8_t ulx_st_SOLENOID[] = { 0, 1, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_SPI[] = { PH_EH, PH_S, PH_P, PH_IY, PH_AY };
static const uint8_t ulx_st_SPI[] = { 1, 0, 0, 1, 1 };
static const uint8_t ulx_ph_SWD[] = { PH_EH, PH_S, PH_D, PH_AH, PH_B, PH_AH, PH_L, PH_Y, PH_UW, PH_D, PH_IY };
static const uint8_t ulx_st_SWD[] = { 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1 };
static const uint8_t ulx_ph_THERMISTOR[] = { PH_TH, PH_ER, PH_M, PH_IH, PH_S, PH_T, PH_ER };
static const uint8_t ulx_st_THERMISTOR[] = { 0, 0, 0, 1, 0, 0, 0 };
static const uint8_t ulx_ph_THYRISTOR[] = { PH_TH, PH_AY, PH_R, PH_IH, PH_S, PH_T, PH_ER };
static const uint8_t ulx_st_THYRISTOR[] = { 0, 0, 0, 1, 0, 0, 0 };
static const uint8_t ulx_ph_TIMESTAMP[] = { PH_T, PH_AY, PH_M, PH_S, PH_T, PH_AE, PH_M, PH_P };
static const uint8_t ulx_st_TIMESTAMP[] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_TOUCHSCREEN[] = { PH_T, PH_AH, PH_CH, PH_S, PH_K, PH_R, PH_IY, PH_N };
static const uint8_t ulx_st_TOUCHSCREEN[] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_TRIAC[] = { PH_T, PH_R, PH_AY, PH_AE, PH_K };
static const uint8_t ulx_st_TRIAC[] = { 0, 0, 1, 0, 0 };
static const uint8_t ulx_ph_UNDERFLOW[] = { PH_AH, PH_N, PH_D, PH_ER, PH_F, PH_L, PH_OW };
static const uint8_t ulx_st_UNDERFLOW[] = { 1, 0, 0, 0, 0, 0, 0 };
static const uint8_t ulx_ph_USART[] = { PH_Y, PH_UW, PH_S, PH_AA, PH_R, PH_T };
static const uint8_t ulx_st_USART[] = { 0, 1, 0, 0, 0, 0 };
static const uint8_t ulx_ph_VARISTOR[] = { PH_V, PH_AA, PH_R, PH_IH, PH_S, PH_T, PH_ER };
static const uint8_t ulx_st_VARISTOR[] = { 0, 0, 0, 1, 0, 0, 0 };

typedef struct {
    const char *word;
    const uint8_t *phones;
    const uint8_t *stress;
    uint8_t count;
} UserLexEntry;

static const UserLexEntry user_lexicon[65] = {
    { "AMPS", ulx_ph_AMPS, ulx_st_AMPS, 4 },
    { "ANTICLOCKWISE", ulx_ph_ANTICLOCKWISE, ulx_st_ANTICLOCKWISE, 11 },
    { "BAOCHIP", ulx_ph_BAOCHIP, ulx_st_BAOCHIP, 5 },
    { "BOOTLOADER", ulx_ph_BOOTLOADER, ulx_st_BOOTLOADER, 7 },
    { "CALIBRATING", ulx_ph_CALIBRATING, ulx_st_CALIBRATING, 10 },
    { "CHECKSUM", ulx_ph_CHECKSUM, ulx_st_CHECKSUM, 6 },
    { "COULOMB", ulx_ph_COULOMB, ulx_st_COULOMB, 5 },
    { "DABAO", ulx_ph_DABAO, ulx_st_DABAO, 4 },
    { "EEPROM", ulx_ph_EEPROM, ulx_st_EEPROM, 6 },
    { "ENCODER", ulx_ph_ENCODER, ulx_st_ENCODER, 6 },
    { "FARAD", ulx_ph_FARAD, ulx_st_FARAD, 5 },
    { "GIGAHERTZ", ulx_ph_GIGAHERTZ, ulx_st_GIGAHERTZ, 8 },
    { "GPIO", ulx_ph_GPIO, ulx_st_GPIO, 6 },
    { "GUVARI", ulx_ph_GUVARI, ulx_st_GUVARI, 6 },
    { "HALFWORD", ulx_ph_HALFWORD, ulx_st_HALFWORD, 6 },
    { "HDMI", ulx_ph_HDMI, ulx_st_HDMI, 7 },
    { "INDUCTORS", ulx_ph_INDUCTORS, ulx_st_INDUCTORS, 8 },
    { "JTAG", ulx_ph_JTAG, ulx_st_JTAG, 5 },
    { "KILOHERTZ", ulx_ph_KILOHERTZ, ulx_st_KILOHERTZ, 8 },
    { "KILOHM", ulx_ph_KILOHM, ulx_st_KILOHM, 5 },
    { "KILOVOLT", ulx_ph_KILOVOLT, ulx_st_KILOVOLT, 8 },
    { "KILOVOLTS", ulx_ph_KILOVOLTS, ulx_st_KILOVOLTS, 9 },
    { "LEDS", ulx_ph_LEDS, ulx_st_LEDS, 6 },
    { "LITRE", ulx_ph_LITRE, ulx_st_LITRE, 4 },
    { "LITRES", ulx_ph_LITRES, ulx_st_LITRES, 5 },
    { "MEGOHM", ulx_ph_MEGOHM, ulx_st_MEGOHM, 5 },
    { "MICROAMP", ulx_ph_MICROAMP, ulx_st_MICROAMP, 8 },
    { "MICROCONTROLLER", ulx_ph_MICROCONTROLLER, ulx_st_MICROCONTROLLER, 13 },
    { "MICROFARAD", ulx_ph_MICROFARAD, ulx_st_MICROFARAD, 10 },
    { "MICROHENRY", ulx_ph_MICROHENRY, ulx_st_MICROHENRY, 10 },
    { "MICROSECOND", ulx_ph_MICROSECOND, ulx_st_MICROSECOND, 11 },
    { "MICROSECONDS", ulx_ph_MICROSECONDS, ulx_st_MICROSECONDS, 12 },
    { "MICROVOLT", ulx_ph_MICROVOLT, ulx_st_MICROVOLT, 9 },
    { "MICROWATT", ulx_ph_MICROWATT, ulx_st_MICROWATT, 8 },
    { "MILLIAMP", ulx_ph_MILLIAMP, ulx_st_MILLIAMP, 7 },
    { "MILLIAMPS", ulx_ph_MILLIAMPS, ulx_st_MILLIAMPS, 8 },
    { "MILLIHENRY", ulx_ph_MILLIHENRY, ulx_st_MILLIHENRY, 9 },
    { "MILLIOHM", ulx_ph_MILLIOHM, ulx_st_MILLIOHM, 6 },
    { "MILLIVOLT", ulx_ph_MILLIVOLT, ulx_st_MILLIVOLT, 8 },
    { "MILLIVOLTS", ulx_ph_MILLIVOLTS, ulx_st_MILLIVOLTS, 9 },
    { "MILLIWATT", ulx_ph_MILLIWATT, ulx_st_MILLIWATT, 7 },
    { "MOSFET", ulx_ph_MOSFET, ulx_st_MOSFET, 6 },
    { "NANOFARAD", ulx_ph_NANOFARAD, ulx_st_NANOFARAD, 9 },
    { "PHOTODIODE", ulx_ph_PHOTODIODE, ulx_st_PHOTODIODE, 8 },
    { "PHOTOTRANSISTOR", ulx_ph_PHOTOTRANSISTOR, ulx_st_PHOTOTRANSISTOR, 13 },
    { "PICOFARAD", ulx_ph_PICOFARAD, ulx_st_PICOFARAD, 9 },
    { "PIEZO", ulx_ph_PIEZO, ulx_st_PIEZO, 5 },
    { "POTENTIOMETER", ulx_ph_POTENTIOMETER, ulx_st_POTENTIOMETER, 12 },
    { "QSPI", ulx_ph_QSPI, ulx_st_QSPI, 8 },
    { "RESONATOR", ulx_ph_RESONATOR, ulx_st_RESONATOR, 8 },
    { "RHEOSTAT", ulx_ph_RHEOSTAT, ulx_st_RHEOSTAT, 7 },
    { "ROVARI", ulx_ph_ROVARI, ulx_st_ROVARI, 6 },
    { "SD", ulx_ph_SD, ulx_st_SD, 4 },
    { "SERVOS", ulx_ph_SERVOS, ulx_st_SERVOS, 5 },
    { "SOLENOID", ulx_ph_SOLENOID, ulx_st_SOLENOID, 7 },
    { "SPI", ulx_ph_SPI, ulx_st_SPI, 5 },
    { "SWD", ulx_ph_SWD, ulx_st_SWD, 11 },
    { "THERMISTOR", ulx_ph_THERMISTOR, ulx_st_THERMISTOR, 7 },
    { "THYRISTOR", ulx_ph_THYRISTOR, ulx_st_THYRISTOR, 7 },
    { "TIMESTAMP", ulx_ph_TIMESTAMP, ulx_st_TIMESTAMP, 8 },
    { "TOUCHSCREEN", ulx_ph_TOUCHSCREEN, ulx_st_TOUCHSCREEN, 8 },
    { "TRIAC", ulx_ph_TRIAC, ulx_st_TRIAC, 5 },
    { "UNDERFLOW", ulx_ph_UNDERFLOW, ulx_st_UNDERFLOW, 7 },
    { "USART", ulx_ph_USART, ulx_st_USART, 6 },
    { "VARISTOR", ulx_ph_VARISTOR, ulx_st_VARISTOR, 7 },
};

#define USER_LEXICON_COUNT 65

#endif /* USER_LEXICON_H */
