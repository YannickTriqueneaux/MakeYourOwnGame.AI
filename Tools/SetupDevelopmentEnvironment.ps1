param(
    [Parameter(Mandatory = $true)]
    [string] $RepositoryRoot,
    [switch] $ChangeAssistant
)

$ErrorActionPreference = "Stop"
$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$assistantSettingsPath = Join-Path $RepositoryRoot `
    "AssistantHost\Config\settings.json"
$assistantSettingsExamplePath = Join-Path $RepositoryRoot `
    "AssistantHost\Config\settings.example.json"
$openAISettingsPath = Join-Path $RepositoryRoot `
    "AssistantProviders\OpenAI\OpenAIProvider.settings.json"
$openAISettingsExamplePath = Join-Path $RepositoryRoot `
    "AssistantProviders\OpenAI\OpenAIProvider.settings.example.json"
$configureOpenAIPath = Join-Path $RepositoryRoot `
    "AssistantHost\Config\ConfigureOpenAI.ps1"
$previousAssistantSettings = $null
if ($ChangeAssistant -and (Test-Path -LiteralPath $assistantSettingsPath)) {
    $previousAssistantSettings = Get-Content `
        -LiteralPath $assistantSettingsPath -Raw
}

function Cancel-AssistantChange {
    param([string] $Message = "Assistant change cancelled.")

    if ($ChangeAssistant -and $null -ne $previousAssistantSettings) {
        [System.IO.File]::WriteAllText(
            $assistantSettingsPath,
            $previousAssistantSettings,
            [System.Text.UTF8Encoding]::new($false))
        Write-Host "Previous assistant configuration restored."
    }
    Write-Host $Message
    exit 2
}

function Request-DifferentAssistant {
    param([string] $Message = "Assistant sign-in cancelled.")

    if ($null -ne $previousAssistantSettings) {
        [System.IO.File]::WriteAllText(
            $assistantSettingsPath,
            $previousAssistantSettings,
            [System.Text.UTF8Encoding]::new($false))
        Write-Host "Previous assistant configuration restored."
    }
    throw [System.OperationCanceledException]::new($Message)
}

function Read-Confirmation {
    param([string] $Prompt)

    while ($true) {
        $answer = Read-Host "$Prompt [Y/n]"
        if ([string]::IsNullOrWhiteSpace($answer) -or $answer -match '^[Yy]') {
            return $true
        }
        if ($answer -match '^[Nn]') {
            return $false
        }
        Write-Host "Please answer Y or N."
    }
}

function Update-ProcessPath {
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machinePath;$userPath"
}

function Find-CommandPath {
    param([string] $Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $command) {
        return $null
    }
    return $command.Source
}

function Require-Winget {
    if ($null -ne (Find-CommandPath "winget.exe")) {
        return
    }
    throw "Windows Package Manager (winget) is required to install missing development tools. Install App Installer from Microsoft, then run Start.bat again."
}

function Install-WingetPackage {
    param(
        [string] $DisplayName,
        [string] $PackageId,
        [string] $Override = ""
    )

    if (-not (Read-Confirmation "$DisplayName is missing. Install it now?")) {
        throw "$DisplayName is required to build the project."
    }
    Require-Winget
    Write-Host "Installing $DisplayName..."
    $arguments = @(
        "install", "--id", $PackageId, "-e",
        "--accept-package-agreements", "--accept-source-agreements"
    )
    if (-not [string]::IsNullOrWhiteSpace($Override)) {
        $arguments += @("--override", $Override)
    }
    & winget.exe @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$DisplayName installation failed with exit code $LASTEXITCODE."
    }
    Update-ProcessPath
}

function Get-MsvcInstallation {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        return $null
    }
    $installation = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installation)) {
        return $null
    }
    return [string] $installation
}

function Ensure-AssistantSettings {
    if ((Test-Path -LiteralPath $assistantSettingsPath) -and
        -not $ChangeAssistant) {
        try {
            $settings = Get-Content -LiteralPath $assistantSettingsPath -Raw |
                ConvertFrom-Json
            $provider = [string] $settings.assistant.providerLibrary
            if ($provider -notin @(
                    "AssistantProviderCodex.dll",
                    "AssistantProviderClaude.dll",
                    "AssistantProviderOpenAI.dll")) {
                throw "Unsupported assistant provider: $provider"
            }
            Write-Host "Assistant provider already configured: $provider"
            return $settings
        }
        catch {
            throw "Assistant configuration is invalid: $($_.Exception.Message)"
        }
    }

    if (-not (Test-Path -LiteralPath $assistantSettingsExamplePath)) {
        throw "Assistant settings example is missing."
    }
    Write-Host ""
    Write-Host "Choose the AI assistant provider:"
    Write-Host "  1. Codex CLI with a ChatGPT account (recommended)"
    Write-Host "  2. Claude Code with a Claude or Anthropic Console account"
    Write-Host "  3. OpenAI API with usage-based billing"
    if ($ChangeAssistant) {
        Write-Host "  C. Cancel and keep the current assistant"
    }
    $selection = Read-Host "Provider [1]"
    if ([string]::IsNullOrWhiteSpace($selection)) {
        $selection = "1"
    }
    switch ($selection) {
        { $ChangeAssistant -and $_ -match '^[Cc]$' } {
            Cancel-AssistantChange
        }
        "1" {
            $settings = @{
                assistant = @{
                    providerLibrary = "AssistantProviderCodex.dll"
                    providerSettings = "CodexProvider.settings.json"
                    promptConfig = "AssistantPrompts.json"
                }
            }
        }
        "2" {
            $settings = @{
                assistant = @{
                    providerLibrary = "AssistantProviderClaude.dll"
                    providerSettings = "ClaudeProvider.settings.json"
                    promptConfig = "AssistantPrompts.json"
                }
            }
        }
        "3" {
            $settings = @{
                assistant = @{
                    providerLibrary = "AssistantProviderOpenAI.dll"
                    providerSettings = "OpenAIProvider.settings.json"
                    promptConfig = "AssistantPrompts.json"
                }
            }
        }
        default { throw "Invalid assistant provider selection." }
    }
    $settings | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath $assistantSettingsPath -Encoding UTF8
    Write-Host "Assistant provider configuration saved."
    return $settings
}

function Find-Codex {
    $candidates = @(
        (Join-Path $env:USERPROFILE ".local\bin\codex.exe")
    )
    $commands = Get-Command "codex" -All -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandType -in @("Application", "ExternalScript") }
    foreach ($command in $commands) {
        if (-not [string]::IsNullOrWhiteSpace($command.Source)) {
            $candidates += $command.Source
        }
    }
    $candidates += Join-Path $env:APPDATA "npm\codex.cmd"

    $seen = @{}
    $firstCandidate = $null
    $installedCandidates = @()
    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate) -or
            -not (Test-Path -LiteralPath $candidate)) {
            continue
        }
        $key = $candidate.ToLowerInvariant()
        if ($seen.ContainsKey($key)) {
            continue
        }
        $seen[$key] = $true
        if ($null -eq $firstCandidate) {
            $firstCandidate = $candidate
        }

        try {
            $versionOutput = @(& $candidate --version 2>&1) |
                ForEach-Object { $_.ToString() }
        }
        catch {
            continue
        }
        $versionText = $versionOutput -join " "
        $version = [version] "0.0"
        $versionMatch = [regex]::Match(
            $versionText,
            "(?i)codex-cli\s+(\d+\.\d+\.\d+)(-[0-9A-Za-z.-]+)?")
        $isPrerelease = $false
        if ($versionMatch.Success) {
            $isPrerelease = -not [string]::IsNullOrWhiteSpace(
                $versionMatch.Groups[2].Value)
            try {
                $version = [version] $versionMatch.Groups[1].Value
            }
            catch {
                # Keep an unparseable version below any recognizable version.
            }
        }
        $installedCandidates += [pscustomobject] @{
            Path = $candidate
            Version = $version
            IsPrerelease = $isPrerelease
        }
    }
    if ($installedCandidates.Count -gt 0) {
        $newest = $installedCandidates |
            Sort-Object -Property @(
                @{ Expression = "Version"; Descending = $true },
                @{ Expression = "IsPrerelease"; Descending = $false }
            ) |
            Select-Object -First 1
        Write-Host "Using newest installed Codex CLI: $($newest.Path) ($($newest.Version))"
        return $newest.Path
    }
    return $firstCandidate
}

function Install-OfficialCodex {
    Write-Host "Downloading and running the official Codex CLI installer..."
    $installer = Invoke-RestMethod -Uri `
        "https://chatgpt.com/codex/install.ps1"
    $previousNonInteractive = $env:CODEX_NON_INTERACTIVE
    try {
        # Start.bat owns the setup flow. Prevent the upstream installer from
        # launching an unrelated interactive Codex session after installation.
        $env:CODEX_NON_INTERACTIVE = "1"
        Invoke-Expression $installer
    }
    finally {
        if ($null -eq $previousNonInteractive) {
            Remove-Item Env:CODEX_NON_INTERACTIVE -ErrorAction SilentlyContinue
        } else {
            $env:CODEX_NON_INTERACTIVE = $previousNonInteractive
        }
    }
    Update-ProcessPath
}

function Ensure-Codex {
    $codex = Find-Codex
    if ($null -eq $codex) {
        if (-not (Read-Confirmation "Codex CLI is missing. Install it using the official OpenAI installer?")) {
            throw "Codex CLI is required by the selected assistant provider."
        }
        Install-OfficialCodex
        $codex = Find-Codex
        if ($null -eq $codex) {
            throw "Codex installation completed, but the executable could not be found."
        }
    } else {
        Write-Host "Codex CLI already installed: $codex"
    }

    & $codex --version
    if ($LASTEXITCODE -ne 0) {
        throw "Codex CLI is installed but could not be started."
    }

    Write-Host "Codex CLI detected. Start.bat will use its existing account session."
}

function Find-Claude {
    $candidates = @(
        (Join-Path $env:USERPROFILE ".local\bin\claude.exe"),
        (Join-Path $env:APPDATA "npm\claude.cmd")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    return Find-CommandPath "claude"
}

function Install-OfficialClaude {
    Write-Host "Downloading and running the official Claude Code installer..."
    $installer = Invoke-RestMethod -Uri "https://claude.ai/install.ps1"
    Invoke-Expression $installer
    Update-ProcessPath
}

function Get-ClaudeAuthStatus {
    param([string] $ClaudePath)

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& $ClaudePath auth status --text 2>&1) |
            ForEach-Object { $_.ToString() }
        return @{
            Success = $LASTEXITCODE -eq 0
            Output = ($output -join [Environment]::NewLine).Trim()
        }
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

function Ensure-Claude {
    $claude = Find-Claude
    if ($null -eq $claude) {
        if (-not (Read-Confirmation "Claude Code is missing. Install it using the official Anthropic installer?")) {
            throw "Claude Code is required by the selected assistant provider."
        }
        Install-OfficialClaude
        $claude = Find-Claude
        if ($null -eq $claude) {
            throw "Claude Code installation completed, but the executable could not be found."
        }
    } else {
        Write-Host "Claude Code already installed: $claude"
    }

    & $claude --version
    if ($LASTEXITCODE -ne 0) {
        throw "Claude Code is installed but could not be started."
    }

    $authStatus = Get-ClaudeAuthStatus -ClaudePath $claude
    if (-not $authStatus.Success) {
        Write-Host ""
        Write-Host "Claude Code requires Claude Pro, Max, Team, Enterprise, or an Anthropic Console account with billing."
        Write-Host "The sign-in page will open in your browser."
        if (-not (Read-Confirmation "Continue with Claude Code sign-in?")) {
            Request-DifferentAssistant
        }
        Write-Host "Press Ctrl+C if you need to cancel the Claude Code sign-in."
        & $claude auth login
        if ($LASTEXITCODE -ne 0) {
            if ($ChangeAssistant) {
                Request-DifferentAssistant `
                    -Message "Claude Code sign-in was cancelled or failed."
            }
            throw "Claude Code sign-in was cancelled or failed."
        }
        $authStatus = Get-ClaudeAuthStatus -ClaudePath $claude
        if (-not $authStatus.Success) {
            throw "Claude Code did not report a valid authenticated session: $($authStatus.Output)"
        }
    } else {
        Write-Host "Claude Code authentication already configured."
    }
}

Write-Host "Checking the development environment..."
Update-ProcessPath

if ($null -eq (Find-CommandPath "git.exe")) {
    Install-WingetPackage -DisplayName "Git" -PackageId "Git.Git"
} else {
    Write-Host "Git already installed."
}

if ($null -eq (Find-CommandPath "cmake.exe")) {
    Install-WingetPackage -DisplayName "CMake" -PackageId "Kitware.CMake"
} else {
    Write-Host "CMake already installed."
}

if ($null -eq (Get-MsvcInstallation)) {
    Install-WingetPackage `
        -DisplayName "Visual Studio 2022 Build Tools with C++ support" `
        -PackageId "Microsoft.VisualStudio.2022.BuildTools" `
        -Override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    if ($null -eq (Get-MsvcInstallation)) {
        throw "Visual Studio Build Tools installed, but the C++ workload was not detected."
    }
} else {
    Write-Host "Visual Studio C++ Build Tools already installed."
}

if (-not (Test-Path -LiteralPath $openAISettingsPath)) {
    if (-not (Test-Path -LiteralPath $openAISettingsExamplePath)) {
        throw "OpenAI provider settings example is missing."
    }
    Copy-Item -LiteralPath $openAISettingsExamplePath `
        -Destination $openAISettingsPath
    Write-Host "Created local OpenAI provider settings."
}

while ($true) {
    $assistantSettings = Ensure-AssistantSettings
    $selectedProvider = [string] $assistantSettings.assistant.providerLibrary
    try {
        if ($selectedProvider -eq "AssistantProviderCodex.dll") {
            Ensure-Codex
        } elseif ($selectedProvider -eq "AssistantProviderClaude.dll") {
            Ensure-Claude
        } elseif ($selectedProvider -eq "AssistantProviderOpenAI.dll") {
            & $configureOpenAIPath -SettingsPath $assistantSettingsPath
            if ($LASTEXITCODE -ne 0) {
                Request-DifferentAssistant `
                    -Message "OpenAI API configuration was cancelled or failed."
            }
        }
        break
    }
    catch [System.OperationCanceledException] {
        Write-Host $_.Exception.Message
        Write-Host "Choose another assistant, or enter C to cancel setup."
        $ChangeAssistant = $true
    }
}

Write-Host "Development environment is ready."
