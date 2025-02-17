#ifndef OPTION_CONFIG_H
#define OPTION_CONFIG_H
#pragma once


#include <vector>
#include <string>
#include "FastLED.h"

const std::vector<uint8_t> UID_UEBERLEBENDER = {0x4, 0xAD, 0xD1, 0xCA, 0x6E, 0x67, 0x80}; // Überlebender
const std::vector<uint8_t> UID_KRIEGER = {0x4, 0xB1, 0xD1, 0xCA, 0x6E, 0x67, 0x80}; // Krieger
const std::vector<uint8_t> UID_WISSENDE = {0x4, 0xA9, 0xD1, 0xCA, 0x6E, 0x67, 0x80}; // Wissende
const std::vector<uint8_t> UID_ABENTEURERIN = {0x4, 0xA5, 0xD1, 0xCA, 0x6E, 0x67, 0x80}; // Abenteurerin
const std::vector<uint8_t> UID_NARR = {0x4, 0xA1, 0xD1, 0xCA, 0x6E, 0x67, 0x80}; // Narr
const std::vector<uint8_t> UID_PUPPENSPIELERIN = {0x4, 0x9D, 0xD1, 0xCA, 0x6E, 0x67, 0x80}; // Puppenspielerin

const std::vector<uint8_t> UID1 = {0x4, 0x6F, 0xD1, 0xCA, 0x6E, 0x67, 0x80};
const std::vector<uint8_t> UID2 = {0x4, 0x6A, 0xD1, 0xCA, 0x6E, 0x67, 0x80};
const std::vector<uint8_t> UID3 = {0x4, 0x64, 0xD1, 0xCA, 0x6E, 0x67, 0x80};
const std::vector<uint8_t> UID4 = {0x4, 0x60, 0xD1, 0xCA, 0x6E, 0x67, 0x80};
const std::vector<uint8_t> UID5 = {0x4, 0x5C, 0xD1, 0xCA, 0x6E, 0x67, 0x80};
const std::vector<uint8_t> UID6 = {0x4, 0x58, 0xD1, 0xCA, 0x6E, 0x67, 0x80};

enum ColLetter
{
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I
};

class ButtonPair
{
public:
    std::string symbol;
    std::string category;
    const TProgmemRGBPalette16* color;
    int row;
    ColLetter col;
    int audioFile;

    ButtonPair(std::string sym, std::string cat, const TProgmemRGBPalette16* color, int r, ColLetter c, int audio);
};

class OptionConfig
{
private:
    const std::vector<uint8_t> rfidID;
    std::string hegemon;
    std::vector<ButtonPair> buttons;
    

public:
    OptionConfig(const std::vector<uint8_t> uid, std::string heg, std::vector<ButtonPair> btns, int audioFile);
    int optionAudioFile;
    bool isRFIDequal(std::array<uint8_t, 7>& inputKeystone);
    std::string getHegemon() const;
    const std::vector<ButtonPair> &getButtons() const;
    int getOptionAudioFile() const;
    const ButtonPair* isValidCrystal(int row, ColLetter col) const;
};

#endif // OPTION_CONFIG_H
