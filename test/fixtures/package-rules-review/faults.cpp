#include "GameConfiguration/PackageRules.h"
#include <iostream>
#include <string>
using namespace larp::configuration;
class Source : public IPackageByteSource {
  public:
    Source(const std::string &s, std::size_t f) : s_(s), f_(f) {}
    PackageReadStatus readByte(std::uint8_t &b) override {
        if (i_ == f_)
            return PackageReadStatus::Failure;
        if (i_ == s_.size())
            return PackageReadStatus::End;
        b = static_cast<std::uint8_t>(s_[i_++]);
        return PackageReadStatus::Byte;
    }

  private:
    const std::string &s_;
    std::size_t f_, i_ = 0;
};
int main() {
    PackageRuleReader reader;
    std::string s;
    std::size_t checks = 0;
    while (std::getline(std::cin, s)) {
        for (std::size_t i = 0; i <= s.size(); ++i) {
            Source source(s, i);
            auto r = reader.validate(source);
            ++checks;
            if (r.scope != RuleScope::Rejected ||
                r.declarations.canonical.error != CanonicalJsonError::InputFailure ||
                r.offset != i) {
                std::cerr << "failure at " << i << " error " << int(r.error) << " canonical "
                          << int(r.declarations.canonical.error) << " offset " << r.offset << "\n";
                return 1;
            }
            if (i < s.size()) {
                auto t = reader.validate(s.data(), i);
                ++checks;
                if (t.scope != RuleScope::Rejected) {
                    std::cerr << "prefix accepted " << i << "\n";
                    return 2;
                }
            }
        }
    }
    std::cout << checks << " source/prefix checks passed\n";
}
