#include "OptionConfig.h"
#include "twinklefox.h"
using namespace fl;

/*
UID #1: 4 6F D1 CA 6E 67 80
UID #2: 4 6A D1 CA 6E 67 80
UID #3: 4 64 D1 CA 6E 67 80
UID #4: 4 60 D1 CA 6E 67 80 
UID #5: 4 5C D1 CA 6E 67 80
UID #6: 4 58 D1 CA 6E 67 80
*/
std::vector<OptionConfig> getDefaultOptions()
{
    return {
        OptionConfig(UID_PUPPENSPIELERIN, "Puppenspieler:in", {
                    ButtonPair("Spinne", "Tier", nullptr, 1, A, 132), // symbol, category, row, col, audioFile
                                                ButtonPair("Marionette", "Gegenstand", nullptr, 1, H, 131),
                                                ButtonPair("Lila", "Farbe", &Purple_p, 4, E, 130),
                                            },
                     230),

        OptionConfig(UID_KRIEGER, "Krieger:in", {
                                          ButtonPair("Schild", "Gegenstand", nullptr, 2, B, 112),
                                          ButtonPair("Rot", "Farbe", &Red_p, 6, B, 111),
                                          ButtonPair("Löwe", "Tier", nullptr, 3, F, 110),
                                      },
                     210),

        OptionConfig(UID_NARR, "Narr / Närrin", {
                                             ButtonPair("Käfig", "Gegenstand", nullptr, 2, C, 120),
                                             ButtonPair("Orange", "Farbe", &Orange_p, 9, B, 121),
                                             ButtonPair("Rabe", "Tier", nullptr, 9, H, 122),
                                         },
                     220),

        OptionConfig(UID_ABENTEURERIN, "Abenteurer:in", {
                                             ButtonPair("Seil", "Gegenstand", nullptr, 5, H, 102),
                                             ButtonPair("Grün", "Farbe", &Green_p, 2, E, 101),
                                             ButtonPair("Fuchs", "Tier", nullptr, 2, F, 100),
                                         },
                     200),
        OptionConfig(UID_UEBERLEBENDER, "Überlebende:r", {
                                             ButtonPair("Schwer", "Gegenstand", nullptr, 9, D, 150),
                                             ButtonPair("Weiß", "Farbe", &White_p, 7, G, 151),
                                             ButtonPair("Ratte", "Tier", nullptr, 3, G, 152),
                                         },
                     250),
        OptionConfig(UID_WISSENDE, "Wissende:r", {
                                             ButtonPair("Buch", "Gegenstand", nullptr, 9, F, 141),
                                             ButtonPair("Blau  ", "Farbe", &Blue_p, 2, G, 140),
                                             ButtonPair("Pfau", "Tier", nullptr, 8, E, 142),
                                         },
                     240),
    };
}