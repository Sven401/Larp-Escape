#pragma once
#include "GamePorts/IGameDiagnostics.h"

class SerialGameDiagnostics : public larp::ports::IGameDiagnostics {
  public:
    void report(larp::ports::GameDiagnosticEvent event, std::uint32_t value,
                std::uint32_t auxiliary) override;
};
