#pragma once
#include "GameConfiguration/Profile.h"
#include <cstdio>
// Host-only graph preview. Scripted values and traced actions; no device drivers.
int runPreview(const larp::configuration::Profile &profile, std::FILE *input);
