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
                         ButtonPair("Button1", "Pair1", &Blue_p, 7, B, 101), 
                         ButtonPair("Button2", "Pair1", &Blue_p, 8, H, 103), 
                         ButtonPair("Button3", "Pair1", &Blue_p, 2, H, 109),
                     },
                     110), // generic solved sound)
        OptionConfig(UID2, "SecRinsui",
                     {
                         // loggingsymbol, loggingcategory, color ,row, col, audioFile
                         ButtonPair("Button1", "Pair2", &Red_p, 2, C, 204),
                         ButtonPair("Button2", "Pair2", &Red_p, 5, A, 206),
                         ButtonPair("undefined", "Pair2", &Red_p, 4, I, 207),
                     },
                     210), // generic solved sound)
        OptionConfig(UID3, "|||",
                     {
                         // loggingsymbol, loggingcategory, color ,row, col, audioFile
                         ButtonPair("Button1", "Pair3", &Yellow_p, 8, F, 302),
                         ButtonPair("Button2", "Pair3", &Yellow_p, 6, G, 305),
                         ButtonPair("Button3", "Pair3", &Yellow_p, 1, D, 308),
                     },
                     310), // generic solved sound)

    };
}