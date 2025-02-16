#include "OptionConfig.h"
#include "twinklefox.h"
using namespace fl;

std::vector<OptionConfig> getDefaultOptions()
{
    return {
        // uint8_t id, std::string heg, std::vector<ButtonPair> btns, int audioFile
        OptionConfig(1, "Puppenspieler:in", {
                                                ButtonPair("Spinne", "Tier", nullptr, 1, A, 132), // symbol, category, row, col, audioFile
                                                ButtonPair("Marionette", "Gegenstand", nullptr, 1, H, 131),
                                                ButtonPair("Lila", "Farbe", &Purple_p, 4, E, 130),
                                            },
                     230),

        OptionConfig(2, "Krieger:in", {
                                          ButtonPair("Schild", "Gegenstand", nullptr, 2, B, 112),
                                          ButtonPair("Rot", "Farbe", &Red_p, 6, B, 111),
                                          ButtonPair("Löwe", "Tier", nullptr, 3, F, 110),
                                      },
                     210),

        OptionConfig(3, "Narr / Närrin", {
                                             ButtonPair("Käfig", "Gegenstand", nullptr, 2, C, 120),
                                             ButtonPair("Orange", "Farbe", &Orange_p, 9, B, 121),
                                             ButtonPair("Rabe", "Tier", nullptr, 9, H, 122),
                                         },
                     220),

        OptionConfig(3, "Abenteurer:in", {
                                             ButtonPair("Seil", "Gegenstand", nullptr, 5, H, 102),
                                             ButtonPair("Grün", "Farbe", &Green_p, 2, E, 101),
                                             ButtonPair("Fuchs", "Tier", nullptr, 2, F, 100),
                                         },
                     200),
        OptionConfig(3, "Überlebende:r", {
                                             ButtonPair("Schwer", "Gegenstand", nullptr, 9, D, 150),
                                             ButtonPair("Weiß", "Farbe", &White_p, 7, G, 151),
                                             ButtonPair("Ratte", "Tier", nullptr, 3, G, 152),
                                         },
                     220),
        OptionConfig(3, "Wissende:r", {
                                             ButtonPair("Buch", "Gegenstand", nullptr, 9, F, 141),
                                             ButtonPair("Blau  ", "Farbe", &Blue_p, 2, G, 140),
                                             ButtonPair("Pfau", "Tier", nullptr, 8, E, 142),
                                         },
                     220),
    };
}