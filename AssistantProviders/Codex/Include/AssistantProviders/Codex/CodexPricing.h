#pragma once

#include "Development/AssistantProvider.h"

#include <map>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>

namespace AssistantProviders::Codex {
class CodexPricing {
public:
  explicit CodexPricing(const nlohmann::json &pricing);

  [[nodiscard]] Development::AssistantCostEstimate
  EstimateCost(std::string_view model,
               const Development::AssistantTokenUsage &usage) const;

private:
  std::map<std::string, double, std::less<>> averageRates_;
};
} // namespace AssistantProviders::Codex
