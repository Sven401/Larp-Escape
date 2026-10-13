#include "Companion/ProfileResponseQueue.h"
namespace larp {
namespace companion {
namespace {
class Call {
  public:
    explicit Call(bool &v) : v_(v) {
        v_ = true;
    }
    ~Call() {
        v_ = false;
    }

  private:
    bool &v_;
};
} // namespace
ProfileResponseSend ProfileResponseReader::select(std::uint32_t now) noexcept {
    if (busy_ || sender_.pending())
        return ProfileResponseSend::Busy;
    Call call(busy_);
    RoutedProfileResponse value;
    if (!queue_.takeCurrent(value))
        return ProfileResponseSend::Idle;
    return sender_.load(value, now);
}
} // namespace companion
} // namespace larp
