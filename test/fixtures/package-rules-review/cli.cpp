#include "GameConfiguration/PackageRules.h"
#include <iostream>
#include <string>
using namespace larp::configuration;
int main() {
    PackageRuleReader reader;
    std::string line;
    while (std::getline(std::cin, line)) {
        const RuleResult r = reader.validate(line.data(), line.size());
        if (r.scope == RuleScope::DirectRulesOnly &&
            (r.error != RuleError::None ||
             r.declarations.scope != DeclarationScope::DeclarationsOnly ||
             r.declarations.canonical.error != CanonicalJsonError::None ||
             r.offset != line.size())) {
            std::cerr << "Inconsistent accepted result\n";
            return 3;
        }
        const char status = r.scope == RuleScope::DirectRulesOnly         ? 'A'
                            : r.error == RuleError::UnsupportedScope      ? 'U'
                            : r.error == RuleError::OperatingProfileLimit ? 'P'
                            : r.error == RuleError::Conflict              ? 'C'
                                                                          : 'R';
        std::cout << status << " " << int(r.error) << " " << int(r.declarations.canonical.error)
                  << " " << r.offset << " " << r.path << "\n";
    }
}
