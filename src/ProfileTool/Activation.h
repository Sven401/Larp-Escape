#pragma once
#include "GameConfiguration/Profile.h"
// Ordinary host setup workflow with synthetic owners and copied queue traffic.
int runActivation(const larp::configuration::Profile &initial,
                  const larp::configuration::Profile &candidate);
