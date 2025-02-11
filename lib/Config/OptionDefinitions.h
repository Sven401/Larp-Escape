#ifndef OPTION_DEFINITIONS_H
#define OPTION_DEFINITIONS_H

#include "OptionConfig.h"

std::vector<OptionConfig> getDefaultOptions()
{
    return {
        //uint8_t id, std::string heg, std::vector<ButtonPair> btns, int audioFile
        OptionConfig(PUPPENSPIELERID, "Puppenspieler:in", {
                                                              ButtonPair("Spinne", "Tier", nullptr , 1, A, 101),  // symbol, category, row, col, audioFile
                                                              ButtonPair("Marionette", "Gegenstand", nullptr , 1, H, 102),
                                                              ButtonPair("Lila", "Farbe", &Purple_p , 4, E, 103),
                                                          },
                     200),

        OptionConfig(2, "Krieger:in", {
                                          ButtonPair("Schild", "Gegenstand", nullptr, 2, B, 104),
                                          ButtonPair("Rot", "Farbe", nullptr, 6, B, 105),
                                          ButtonPair("Löwe", "Tier", nullptr, 3, F, 106),
                                      },
                     201),

        OptionConfig(3, "Narr / Närrin", {
                                             ButtonPair("Käfig", "Gegenstand", nullptr, 2, C, 107),
                                             ButtonPair("Orange", "Farbe", nullptr, 9, B, 108),
                                             ButtonPair("Rabe", "Tier", nullptr, 9, H, 109),
                                         },
                     202),

        OptionConfig(4, "Fake", {
                                    ButtonPair("Rosa", "Farbe", nullptr, 5, A, 110),
                                    ButtonPair("Stier", "Tier", nullptr, 8, A, 111),
                                    ButtonPair("Türkis", "Farbe", nullptr, 6, H, 112),
                                },
                     203)};
}

#endif // OPTION_DEFINITIONS_H