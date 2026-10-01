#include "OptionConfig.h"
#include "Pallets.h"

using larp::domain::GridColumn;
using larp::domain::GridPosition;

std::vector<OptionConfig> getDefaultOptions()
{
    return {
        OptionConfig(UID_PUPPENSPIELERIN, "Puppenspieler:in", {
            ButtonPair("Spinne", "Tier", nullptr, GridPosition{1, GridColumn::A}, 132),
            ButtonPair("Marionette", "Gegenstand", nullptr, GridPosition{1, GridColumn::H}, 131),
            ButtonPair("Lila", "Farbe", &Purple_p, GridPosition{4, GridColumn::E}, 130),
        }, 230),
        OptionConfig(UID_KRIEGER, "Krieger:in", {
            ButtonPair("Schild", "Gegenstand", nullptr, GridPosition{2, GridColumn::B}, 112),
            ButtonPair("Rot", "Farbe", &Red_p, GridPosition{6, GridColumn::B}, 111),
            ButtonPair("Löwe", "Tier", nullptr, GridPosition{3, GridColumn::F}, 110),
        }, 210),
        OptionConfig(UID_NARR, "Narr / Närrin", {
            ButtonPair("Käfig", "Gegenstand", nullptr, GridPosition{2, GridColumn::C}, 120),
            ButtonPair("Orange", "Farbe", &Orange_p, GridPosition{9, GridColumn::B}, 121),
            ButtonPair("Rabe", "Tier", nullptr, GridPosition{9, GridColumn::H}, 122),
        }, 220),
        OptionConfig(UID_ABENTEURERIN, "Abenteurer:in", {
            ButtonPair("Seil", "Gegenstand", nullptr, GridPosition{5, GridColumn::H}, 102),
            ButtonPair("Grün", "Farbe", &Green_p, GridPosition{2, GridColumn::E}, 101),
            ButtonPair("Fuchs", "Tier", nullptr, GridPosition{2, GridColumn::F}, 100),
        }, 200),
        OptionConfig(UID_UEBERLEBENDER, "Überlebende:r", {
            ButtonPair("Schwer", "Gegenstand", nullptr, GridPosition{9, GridColumn::D}, 150),
            ButtonPair("Weiß", "Farbe", &White_p, GridPosition{7, GridColumn::G}, 151),
            ButtonPair("Ratte", "Tier", nullptr, GridPosition{3, GridColumn::G}, 152),
        }, 250),
        OptionConfig(UID_WISSENDE, "Wissende:r", {
            ButtonPair("Buch", "Gegenstand", nullptr, GridPosition{9, GridColumn::F}, 141),
            ButtonPair("Blau  ", "Farbe", &Blue_p, GridPosition{2, GridColumn::G}, 140),
            ButtonPair("Pfau", "Tier", nullptr, GridPosition{8, GridColumn::E}, 142),
        }, 240),
    };
}
