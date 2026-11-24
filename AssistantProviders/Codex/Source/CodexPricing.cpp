#include "AssistantProviders/Codex/CodexPricing.h"

#include <cmath>
#include <nlohmann/json.hpp>

namespace AssistantProviders::Codex {
CodexPricing::CodexPricing(const nlohmann::json &pricing) {
  if (!pricing.is_object() ||
      pricing.value("basis", "") != "observedAverage" ||
      !pricing.contains("models") || !pricing["models"].is_object())
    return;

  for (const auto &[model, settings] : pricing["models"].items()) {
    if (model.empty() || !settings.is_object() ||
        !settings.contains("usdPerMillionTokens") ||
        !settings["usdPerMillionTokens"].is_number())
      continue;
    const double rate = settings["usdPerMillionTokens"].get<double>();
    if (std::isfinite(rate) && rate >= 0.0)
      averageRates_.emplace(model, rate);
  }
}

Development::AssistantCostEstimate CodexPricing::EstimateCost(
    std::string_view model,
    const Development::AssistantTokenUsage &usage) const {
  auto rate = averageRates_.find(model);
  if (rate == averageRates_.end()) {
    for (auto candidate = averageRates_.begin();
         candidate != averageRates_.end(); ++candidate) {
      if (model.starts_with(candidate->first + "-202")) {
        rate = candidate;
        break;
      }
    }
  }
  if (rate == averageRates_.end())
    return {};

  // Codex inputTokens already includes cached input; do not count it twice.
  const double tokens = static_cast<double>(usage.inputTokens) +
                        static_cast<double>(usage.outputTokens);
  return {.available = true, .usd = tokens / 1'000'000.0 * rate->second};
}
} // namespace AssistantProviders::Codex
