#include "OptionConfig.h"
#include "twinklefox.h"
using namespace fl;

/*
Ids in OptionConfig.h
*/
std::vector<OptionConfig> getDefaultOptions()
{
    return {
        // RFIDID, LoggingName, ReedConfig, SolvedFile
        OptionConfig(UID1, "Quail",
                     {
                         // loggingsymbol, loggingcategory, color ,row, col, audioFile
                         ButtonPair("undefined", "undefined", &Blue_p, 7, B, 101),
                         ButtonPair("undefined", "undefined", &Blue_p, 8, H, 103),
                         ButtonPair("undefined", "undefined", &Blue_p, 9, E, 109),
                     },
                     500), // generic solved sound)
        OptionConfig(UID2, "SecRinsui",
                     {
                         // loggingsymbol, loggingcategory, color ,row, col, audioFile
                         ButtonPair("undefined", "undefined", &Red_p, 2, C, 204),
                         ButtonPair("undefined", "undefined", &Red_p, 5, A, 206),
                         ButtonPair("undefined", "undefined", &Red_p, 4, I, 207),
                     },
                     500), // generic solved sound)
        OptionConfig(UID3, "|||",
                     {
                         // loggingsymbol, loggingcategory, color ,row, col, audioFile
                         ButtonPair("undefined", "undefined", &Yellow_p, 8, F, 302),
                         ButtonPair("undefined", "undefined", &Yellow_p, 6, G, 305),
                         ButtonPair("undefined", "undefined", &Yellow_p, 1, D, 308),
                     },
                     500), // generic solved sound)

    };
}