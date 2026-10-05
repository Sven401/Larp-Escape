#pragma once

#include "GameApplication/IGameSnapshotSink.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// A one-element mailbox: callbacks read copies and never touch the controller.
// Created once before tasks start; it and its queue live until device restart.
class CompanionStatusStore : public larp::application::IGameSnapshotSink {
  public:
    bool begin();
    void publish(const larp::application::GameSnapshot &snapshot) override;
    bool read(larp::application::GameSnapshot &snapshot) const;
    CompanionStatusStore() = default;
    CompanionStatusStore(const CompanionStatusStore &) = delete;
    CompanionStatusStore &operator=(const CompanionStatusStore &) = delete;

  private:
    QueueHandle_t queue_ = nullptr;
};
