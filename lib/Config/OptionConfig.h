#ifndef OPTION_CONFIG_H
#define OPTION_CONFIG_H

#include <vector>
#include <string>
#include "FastLED.h"

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
    uint8_t rfidID;
    std::string hegemon;
    std::vector<ButtonPair> buttons;
    int optionAudioFile;

public:
    OptionConfig(uint8_t id, std::string heg, std::vector<ButtonPair> btns, int audioFile);

    bool isRFIDequal(uint8_t*);
    std::string getHegemon() const;
    const std::vector<ButtonPair> &getButtons() const;
    int getOptionAudioFile() const;
    const ButtonPair* isValidCrystal(int row, ColLetter col) const;
};

#endif // OPTION_CONFIG_H
