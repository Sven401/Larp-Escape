#include "OptionConfig.h"

#include <algorithm>
#include <string>
#include <utility>

ButtonPair::ButtonPair(std::string sym, std::string cat, const TProgmemRGBPalette16 *color,
                       larp::domain::GridPosition position, int audio)
    : symbol(std::move(sym)), category(std::move(cat)), color(color), position(position),
      audioFile(audio)
{
}

OptionConfig::OptionConfig(std::vector<uint8_t> uid, std::string heg, std::vector<ButtonPair> btns,
                           int audioFile)
    : hegemon(std::move(heg)), buttons(std::move(btns)), optionAudioFile(audioFile),
      rfidID(std::move(uid))
{
}

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

const ButtonPair *OptionConfig::isValidCrystal(larp::domain::GridPosition position) const
{
    for (const auto &button : buttons)
    {
        Serial.print("checking against Button: ");
        Serial.print(button.symbol.c_str());
        Serial.print(" at ");
        Serial.print(static_cast<int>(button.position.row));
        Serial.print(" =? ");
        Serial.print(static_cast<int>(position.row));
        Serial.print(" ");
        Serial.println(static_cast<int>(button.position.column));
        Serial.print(" =? ");
        Serial.println(static_cast<int>(position.column));
        if (button.position == position)
        {
            return &button;
        }
    }
    return nullptr;
}
