#pragma once
#include "NfcBleWire.h"
namespace larp { namespace nfcble {
bool begin();
Port &box();
bool busy();
void allow(bool);
} }
