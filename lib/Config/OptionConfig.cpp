#include "OptionConfig.h"
#include <string>

// ButtonPair constructor
ButtonPair::ButtonPair(std::string sym, std::string cat, const TProgmemRGBPalette16 *color, int r, ColLetter c, int audio)
    : symbol(std::move(sym)), category(std::move(cat)), color(color), row(r), col(c), audioFile(audio) {}

// OptionConfig constructor
OptionConfig::OptionConfig(std::vector<uint8_t> uid, std::string heg, std::vector<ButtonPair> btns, int audioFile)
    : hegemon(std::move(heg)), buttons(std::move(btns)), optionAudioFile(audioFile), rfidID(std::move(uid)) {}

// OptionConfig member functions
bool OptionConfig::isRFIDequal(std::array<uint8_t, 7> &inputKeystone)
{
    return rfidID.size() == 7 && std::equal(rfidID.begin(), rfidID.end(), inputKeystone.begin());
}

std::string OptionConfig::getHegemon() const
{
    return hegemon;
}

const std::vector<ButtonPair> &OptionConfig::getButtons() const
{
    return buttons;
}

int OptionConfig::getOptionAudioFile() const
{
    return optionAudioFile;
}

const ButtonPair *OptionConfig::isValidCrystal(int row, ColLetter col) const
{
    for (const auto &button : buttons)
    {
        Serial.print("checking against Button: ");
        Serial.print(button.symbol.c_str());
        Serial.print(" at ");
        Serial.print(button.row);
        Serial.print(" =? ");
        Serial.print(row);
        Serial.print(" "); 
        Serial.println(button.col);
        Serial.print(" =? ");
        Serial.println(col);
        if (button.row == row && button.col == col)
        {
            return &button;
        }
    }
    return nullptr;
}