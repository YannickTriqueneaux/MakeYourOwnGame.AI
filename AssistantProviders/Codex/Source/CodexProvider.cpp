#include "AssistantProviders/Codex/CodexFeatureDetection.h"
#include "AssistantProviders/Codex/CodexEventParser.h"
#include "AssistantProviders/Codex/CodexPricing.h"
#include "Development/AssistantProvider.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#if defined(_WIN32)
#define ASSISTANT_PROVIDER_API extern "C" __declspec(dllexport)
#else
#define ASSISTANT_PROVIDER_API extern "C"
#endif

namespace {
std::vector<unsigned char> DecodeBase64(std::string_view input) {
  static constexpr std::string_view alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<unsigned char> output;
  unsigned int accumulator = 0;
  int bits = -8;
  for (const char character : input) {
    if (character == '=')
      break;
    const auto value = alphabet.find(character);
    if (value == std::string_view::npos)
      continue;
    accumulator = (accumulator << 6) | static_cast<unsigned int>(value);
    bits += 6;
    if (bits >= 0) {
      output.push_back(
          static_cast<unsigned char>((accumulator >> bits) & 0xff));
      bits -= 8;
    }
  }
  return output;
}

std::string ImageExtension(std::string_view mimeType) {
  if (mimeType == "image/jpeg")
    return ".jpg";
  if (mimeType == "image/webp")
    return ".webp";
  return ".png";
}

#if defined(_WIN32)
std::string RunCodexCommand(const std::filesystem::path &executable,
                            std::string_view arguments) {
  const std::string command =
      "cmd /d /s /c \"\"" + executable.string() + "\" " +
      std::string(arguments) + " 2>&1\"";
  std::array<char, 4096> buffer{};
  std::string output;
  FILE *pipe = _popen(command.c_str(), "r");
  if (!pipe)
    return {};
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
    output += buffer.data();
    if (output.size() > 256 * 1024)
      break;
  }
  const int exitCode = _pclose(pipe);
  return exitCode == 0 ? output : std::string{};
}
#endif

std::string ReadCodexFeatureList(const std::filesystem::path &executable) {
#if defined(_WIN32)
  return RunCodexCommand(executable, "features list");
#else
  static_cast<void>(executable);
  return {};
#endif
}

#if defined(_WIN32)
struct CodexCliVersion {
  std::array<std::uint64_t, 3> parts{};
  bool prerelease = false;
};

std::optional<CodexCliVersion>
ParseCodexCliVersion(std::string_view output) {
  const auto marker = output.find("codex-cli");
  if (marker == std::string_view::npos)
    return std::nullopt;
  std::size_t position = marker + std::string_view("codex-cli").size();
  while (position < output.size() &&
         std::isspace(static_cast<unsigned char>(output[position])))
    ++position;

  CodexCliVersion version;
  for (std::size_t index = 0; index < version.parts.size(); ++index) {
    const auto start = position;
    while (position < output.size() &&
           std::isdigit(static_cast<unsigned char>(output[position])))
      ++position;
    if (start == position)
      return std::nullopt;
    const auto [end, error] = std::from_chars(
        output.data() + start, output.data() + position, version.parts[index]);
    if (error != std::errc{} || end != output.data() + position)
      return std::nullopt;
    if (index + 1 < version.parts.size()) {
      if (position >= output.size() || output[position] != '.')
        return std::nullopt;
      ++position;
    }
  }
  version.prerelease = position < output.size() && output[position] == '-';
  return version;
}

bool IsNewerCodexVersion(const CodexCliVersion &candidate,
                         const CodexCliVersion &current) {
  if (candidate.parts != current.parts)
    return candidate.parts > current.parts;
  return current.prerelease && !candidate.prerelease;
}

std::wstring ReadEnvironmentVariable(std::wstring_view name) {
  const std::wstring variable(name);
  std::array<wchar_t, 32768> value{};
  const DWORD length = GetEnvironmentVariableW(
      variable.c_str(), value.data(), static_cast<DWORD>(value.size()));
  if (length == 0 || length >= value.size())
    return {};
  return {value.data(), length};
}

void AddCodexCandidate(std::vector<std::filesystem::path> &candidates,
                       std::filesystem::path candidate) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(candidate, error) || error)
    return;
  auto absolute = std::filesystem::absolute(candidate, error);
  if (!error)
    candidate = std::move(absolute);
  candidate = candidate.lexically_normal();
  auto key = candidate.native();
  std::transform(key.begin(), key.end(), key.begin(),
                 [](wchar_t character) {
                   return static_cast<wchar_t>(std::towlower(character));
                 });
  for (const auto &existing : candidates) {
    auto existingKey = existing.native();
    std::transform(existingKey.begin(), existingKey.end(),
                   existingKey.begin(), [](wchar_t character) {
                     return static_cast<wchar_t>(std::towlower(character));
                   });
    if (existingKey == key)
      return;
  }
  candidates.push_back(std::move(candidate));
}

std::vector<std::filesystem::path> FindCodexCandidates() {
  std::vector<std::filesystem::path> candidates;
  const auto userProfile = ReadEnvironmentVariable(L"USERPROFILE");
  if (!userProfile.empty())
    AddCodexCandidate(candidates, std::filesystem::path(userProfile) /
                                     ".local/bin/codex.exe");

  const auto localAppData = ReadEnvironmentVariable(L"LOCALAPPDATA");
  if (!localAppData.empty())
    AddCodexCandidate(candidates,
                      std::filesystem::path(localAppData) /
                          "Programs/OpenAI/Codex/bin/codex.exe");

  const auto appData = ReadEnvironmentVariable(L"APPDATA");
  if (!appData.empty())
    AddCodexCandidate(candidates,
                      std::filesystem::path(appData) / "npm/codex.cmd");

  const auto pathValue = ReadEnvironmentVariable(L"PATH");
  std::size_t start = 0;
  while (start <= pathValue.size()) {
    const auto end = pathValue.find(L';', start);
    auto directory = pathValue.substr(
        start, end == std::wstring::npos ? std::wstring::npos : end - start);
    if (directory.size() >= 2 && directory.front() == L'"' &&
        directory.back() == L'"') {
      directory = directory.substr(1, directory.size() - 2);
    }
    if (!directory.empty()) {
      const std::filesystem::path folder(directory);
      AddCodexCandidate(candidates, folder / "codex.exe");
      AddCodexCandidate(candidates, folder / "codex.cmd");
      AddCodexCandidate(candidates, folder / "codex.bat");
    }
    if (end == std::wstring::npos)
      break;
    start = end + 1;
  }
  return candidates;
}
#endif

std::filesystem::path ResolveExecutable(std::string configured) {
  if (!configured.empty())
    return configured;
#if defined(_WIN32)
  std::filesystem::path newestExecutable;
  std::optional<CodexCliVersion> newestVersion;
  for (const auto &candidate : FindCodexCandidates()) {
    const auto version =
        ParseCodexCliVersion(RunCodexCommand(candidate, "--version"));
    if (newestExecutable.empty() ||
        (version && (!newestVersion ||
                     IsNewerCodexVersion(*version, *newestVersion)))) {
      newestExecutable = candidate;
      newestVersion = version;
    }
  }
  if (!newestExecutable.empty())
    return newestExecutable;
#endif
  return "codex";
}

class CodexProvider final : public Development::AssistantProvider {
public:
  CodexProvider(const char *settingsPath, const char *gameRoot)
      : gameRoot_(gameRoot) {
    std::ifstream stream(settingsPath);
    nlohmann::json settings;
    stream >> settings;
    executable_ = ResolveExecutable(settings.value("executable", ""));
    mcpExecutable_ = settings.value("gameToolsMcpExecutable", "");
    if (mcpExecutable_.empty())
      mcpExecutable_ = std::filesystem::path(settingsPath).parent_path() /
                       "GameToolsMcpServer.exe";
    model_ = settings.value("model", "");
    reasoningEffort_ = settings.value("reasoningEffort", "high");
    pricing_ = AssistantProviders::Codex::CodexPricing(
        settings.value("pricing", nlohmann::json::object()));
  }

  bool IsConfigured() const override { return !executable_.empty(); }
  const std::string &GetDisplayName() const override { return displayName_; }
  const std::string &GetModel() const override {
    return model_.empty() ? displayName_ : model_;
  }
  Development::AssistantCostEstimate
  EstimateCost(const Development::AssistantTokenUsage &usage) const override {
    return pricing_.EstimateCost(model_, usage);
  }

  Development::AssistantResponse CreateResponse(
      std::string_view instructions, std::string_view prompt,
      const std::vector<Development::AssistantImageInput> &images,
      std::string_view previousResponseId,
      const Development::AssistantStreamCallback &onEvent) const override {
    const auto nonce = std::to_string(GetCurrentProcessId()) + "_" +
                       std::to_string(GetTickCount64());
    const auto promptPath = std::filesystem::temp_directory_path() /
                            ("aitester_codex_" + nonce + ".txt");
    const auto outputPath = std::filesystem::temp_directory_path() /
                            ("aitester_codex_" + nonce + ".out");
    {
      std::ofstream stream(promptPath, std::ios::binary);
      stream << instructions << "\n\n" << prompt;
    }
    std::vector<std::filesystem::path> imagePaths;
    imagePaths.reserve(images.size());
    for (std::size_t index = 0; index < images.size(); ++index) {
      const auto imagePath =
          std::filesystem::temp_directory_path() /
          ("aitester_codex_" + nonce + "_image_" + std::to_string(index) +
           ImageExtension(images[index].mimeType));
      const auto bytes = DecodeBase64(images[index].base64Data);
      std::ofstream image(imagePath, std::ios::binary);
      image.write(reinterpret_cast<const char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
      imagePaths.push_back(imagePath);
    }
    onEvent({Development::AssistantStreamEventType::Status,
             "Codex is working through the ChatGPT account."});
    if (pricing_.EstimateCost(model_, {}).available)
      onEvent({Development::AssistantStreamEventType::Status,
               "Cost estimate uses the configured observed average for " +
                   model_ + "; input, output and cache share one rate."});
    onEvent({Development::AssistantStreamEventType::Status,
             "Checking Codex MCP compatibility."});
    std::call_once(mcpFeatureDetectionOnce_, [this]() {
      enableMcpDiscovery_ = AssistantProviders::Codex::FeatureListContains(
          ReadCodexFeatureList(executable_), "mcp_2026_07_28");
    });
    onEvent({Development::AssistantStreamEventType::Status,
             "Starting required Game tools (up to 10 seconds)."});
    std::string command = "cmd /d /s /c \"\"" + executable_.string() +
                          "\" exec --json --color never --sandbox read-only "
                          "--disable shell_tool --disable unified_exec "
                          "--skip-git-repo-check -C \"" +
                          gameRoot_.string() + "\"";
    if (enableMcpDiscovery_)
      command += " --enable mcp_2026_07_28";
    command += " -c suppress_unstable_features_warning=true";
    std::string mcpPath = mcpExecutable_.generic_string();
    command += " -c \"mcp_servers.game_tools.command='" + mcpPath + "'\"";
    command += " -c mcp_servers.game_tools.required=true";
    command += " -c mcp_servers.game_tools.startup_timeout_sec=10";
    if (!model_.empty())
      command += " --model \"" + model_ + "\"";
    if (!reasoningEffort_.empty())
      command += " -c model_reasoning_effort=\"" + reasoningEffort_ + "\"";
    for (const auto &imagePath : imagePaths)
      command += " --image \"" + imagePath.string() + "\"";
    if (!previousResponseId.empty()) {
      onEvent({Development::AssistantStreamEventType::Status,
               "Resuming the previous Codex conversation."});
      const bool validSessionId =
          previousResponseId.size() <= 128 &&
          std::all_of(previousResponseId.begin(), previousResponseId.end(),
                      [](unsigned char character) {
                        return std::isalnum(character) || character == '-' ||
                               character == '_';
                      });
      if (!validSessionId)
        throw std::runtime_error("Invalid Codex session ID.");
      command +=
          " -c \"mcp_servers.game_tools.args=['--guidance-already-read']\"";
      command += " resume \"" + std::string(previousResponseId) + "\"";
    }
    command += " --output-last-message \"" + outputPath.string() + "\" - < \"" +
               promptPath.string() + "\" 2>&1\"";
    std::array<char, 4096> buffer{};
    std::string pendingLine;
    std::string diagnostics;
    Development::AssistantTokenUsage usage;
    std::string responseId = nonce;
    FILE *pipe = _popen(command.c_str(), "r");
    if (!pipe)
      throw std::runtime_error("Could not start Codex CLI.");
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
      pendingLine += buffer.data();
      std::size_t newline = 0;
      while ((newline = pendingLine.find('\n')) != std::string::npos) {
        std::string line = pendingLine.substr(0, newline);
        pendingLine.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        diagnostics += line + '\n';
        if (diagnostics.size() > 16384)
          diagnostics.erase(0, diagnostics.size() - 16384);
        const auto parsed = AssistantProviders::Codex::ParseEventLine(line);
        for (const auto &event : parsed.events)
          onEvent(event);
        if (parsed.usage)
          usage = *parsed.usage;
        if (!parsed.threadId.empty())
          responseId = parsed.threadId;
      }
    }
    if (!pendingLine.empty()) {
      diagnostics += pendingLine;
      const auto parsed =
          AssistantProviders::Codex::ParseEventLine(pendingLine);
      for (const auto &event : parsed.events)
        onEvent(event);
      if (parsed.usage)
        usage = *parsed.usage;
      if (!parsed.threadId.empty())
        responseId = parsed.threadId;
    }
    const int exitCode = _pclose(pipe);
    std::ifstream output(outputPath, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(output)), {});
    std::error_code error;
    std::filesystem::remove(promptPath, error);
    std::filesystem::remove(outputPath, error);
    for (const auto &imagePath : imagePaths)
      std::filesystem::remove(imagePath, error);
    if (exitCode != 0)
      throw std::runtime_error("Codex CLI failed with exit code " +
                               std::to_string(exitCode) + ". Recent output:\n" +
                               diagnostics);
    if (text.empty())
      throw std::runtime_error("Codex returned no final response.");
    onEvent({Development::AssistantStreamEventType::OutputTextDelta, text});
    return {.id = responseId, .text = text, .usage = usage};
  }

  Development::AssistantResponse ContinueWithToolOutputs(
      std::string_view, const std::vector<Development::AssistantToolOutput> &,
      std::string_view, const Development::AssistantStreamCallback &,
      bool) const override {
    throw std::runtime_error("Codex provider uses its native agent tools.");
  }

private:
  std::filesystem::path gameRoot_;
  std::filesystem::path executable_;
  std::filesystem::path mcpExecutable_;
  std::string model_;
  std::string reasoningEffort_;
  AssistantProviders::Codex::CodexPricing pricing_{
      nlohmann::json::object()};
  mutable std::once_flag mcpFeatureDetectionOnce_;
  mutable bool enableMcpDiscovery_ = false;
  std::string displayName_ = "Codex (ChatGPT account)";
};

Development::AssistantProvider *Create(const char *settingsPath,
                                       const char *gameRoot) {
  try {
    return new CodexProvider(settingsPath, gameRoot);
  } catch (...) {
    return nullptr;
  }
}
void Destroy(Development::AssistantProvider *provider) { delete provider; }
} // namespace

ASSISTANT_PROVIDER_API const Development::AssistantProviderApi *
GetAssistantProviderApi() {
  static const Development::AssistantProviderApi api{
      Development::AssistantProviderApiVersion, &Create, &Destroy};
  return &api;
}
