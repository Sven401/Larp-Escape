#pragma once

#include "GameApplication/GameSnapshot.h"

namespace larp {
namespace application {
class IGameSnapshotSink {
  public:
    virtual ~IGameSnapshotSink() {}
    virtual void publish(const GameSnapshot &snapshot) = 0;
};
} // namespace application
} // namespace larp
