#include "OptionConfig.h"
#include <cstring>

// ButtonPair constructor
ButtonPair::ButtonPair(std::string sym, std::string cat,const TProgmemRGBPalette16* color, int r, ColLetter c, int audio)
    : symbol(std::move(sym)), category(std::move(cat)), color(color) ,row(r), col(c), audioFile(audio) {}

// OptionConfig constructor
OptionConfig::OptionConfig(uint8_t id, std::string heg, std::vector<ButtonPair> btns, int audioFile)
    : rfidID(id), hegemon(std::move(heg)), buttons(std::move(btns)), optionAudioFile(audioFile) {}

// OptionConfig member functions
bool OptionConfig::isRFIDequal(uint8_t* inputKeystone) {
    return memcmp(&rfidID, inputKeystone, sizeof(rfidID)) == 0;
}

std::string OptionConfig::getHegemon() const {
    return hegemon;
}

const std::vector<ButtonPair>& OptionConfig::getButtons() const {
    return buttons;
}

int OptionConfig::getOptionAudioFile() const {
    return optionAudioFile;
}

bool OptionConfig::isValidCrystal(int row, ColLetter col) const {
    for (const auto &button : buttons) {
        if (button.row == row && button.col == col) {
            return true;
        }
    }
    return false;
}