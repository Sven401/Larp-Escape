#include "CompanionStatusStore.h"
#include <type_traits>

static_assert(std::is_trivially_copyable<larp::application::GameSnapshot>::value,
              "FreeRTOS snapshots must be safe value copies");
bool CompanionStatusStore::begin() {
    if (queue_ == nullptr)
        queue_ = xQueueCreate(1U, sizeof(larp::application::GameSnapshot));
    return queue_ != nullptr;
}
void CompanionStatusStore::publish(const larp::application::GameSnapshot &snapshot) {
    if (queue_ != nullptr)
        xQueueOverwrite(queue_, &snapshot);
}
bool CompanionStatusStore::read(larp::application::GameSnapshot &snapshot) const {
    return queue_ != nullptr && xQueuePeek(queue_, &snapshot, 0U) == pdTRUE;
}
