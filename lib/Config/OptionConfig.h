#ifndef OPTION_CONFIG_H
#define OPTION_CONFIG_H

#include <vector>
#include <string>

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
    int row;
    ColLetter col;
    int audioFile;

    ButtonPair(std::string sym, std::string cat, int r, ColLetter c, int audio)
        : symbol(std::move(sym)), category(std::move(cat)), row(r), col(c), audioFile(audio) {}
};

class OptionConfig
{
private:
    int rfidID;
    std::string hegemon;
    std::vector<ButtonPair> buttons;
    int optionAudioFile;

public:
    OptionConfig(int id, std::string heg, std::vector<ButtonPair> btns, int audioFile)
        : rfidID(id), hegemon(std::move(heg)), buttons(std::move(btns)), optionAudioFile(audioFile) {}

    int getRFID() const { return rfidID; }
    std::string getHegemon() const { return hegemon; }
    const std::vector<ButtonPair> &getButtons() const { return buttons; }
    int getOptionAudioFile() const { return optionAudioFile; }

    bool isValidCrystal(int row, ColLetter col) const
    {
        for (const auto &button : buttons)
        {
            if (button.row == row && button.col == col)
            {
                return true;
            }
        }
        return false;
    }
};

#endif // OPTION_CONFIG_H
