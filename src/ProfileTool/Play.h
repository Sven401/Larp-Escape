#pragma once
#include "GameConfiguration/Profile.h"
#include <cstdio>
// Host-only playable session with synthetic device inputs and traced outputs.
int runPlay(const larp::configuration::Profile &profile, std::FILE *input);
