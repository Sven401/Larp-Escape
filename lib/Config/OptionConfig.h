#ifndef OPTION_CONFIG_H
#define OPTION_CONFIG_H
#pragma once


#include <vector>
#include <string>
#include "FastLED.h"


//UID #1: 4 7D D0 CA 6E 67 80
//UID #2: 4 79 D0 CA 6E 67 80
//UID #3: 4 75 D0 CA 6E 67 80

const std::vector<uint8_t> UID1 = {0x4, 0x75, 0xD0, 0xCA, 0x6E, 0x67, 0x80};
const std::vector<uint8_t> UID2 = {0x4, 0x79, 0xD0, 0xCA, 0x6E, 0x67, 0x80};
const std::vector<uint8_t> UID3 = {0x4, 0x75, 0xD0, 0xCA, 0x6E, 0x67, 0x80};

const std::array<int, 5>  ERRORTRACK = {995, 996, 997, 998, 999};
const int BONUSTRACK = 900;

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
