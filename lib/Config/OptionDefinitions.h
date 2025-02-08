#ifndef OPTION_DEFINITIONS_H
#define OPTION_DEFINITIONS_H

#include "OptionConfig.h"

std::vector<OptionConfig> getDefaultOptions()
{
    return {
        OptionConfig(PUPPENSPIELERID, "Puppenspieler:in", {
                                                              ButtonPair("Spinne", "Tier", 1, A, 101),
                                                              ButtonPair("Marionette", "Gegenstand", 1, H, 102),
                                                              ButtonPair("Lila", "Farbe", 4, E, 103),
                                                          },
                     200),

        OptionConfig(2, "Krieger:in", {
                                          ButtonPair("Schild", "Gegenstand", 2, B, 104),
                                          ButtonPair("Rot", "Farbe", 6, B, 105),
                                          ButtonPair("Löwe", "Tier", 3, F, 106),
                                      },
                     201),

        OptionConfig(3, "Narr / Närrin", {
                                             ButtonPair("Käfig", "Gegenstand", 2, C, 107),
                                             ButtonPair("Orange", "Farbe", 9, B, 108),
                                             ButtonPair("Rabe", "Tier", 9, H, 109),
                                         },
                     202),

        OptionConfig(4, "Fake", {
                                    ButtonPair("Rosa", "Farbe", 5, A, 110),
                                    ButtonPair("Stier", "Tier", 8, A, 111),
                                    ButtonPair("Türkis", "Farbe", 6, H, 112),
                                },
                     203)};
}

#endif // OPTION_DEFINITIONS_H