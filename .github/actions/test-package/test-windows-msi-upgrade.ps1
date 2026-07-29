# SPDX-FileCopyrightText: (C) 2026 Ieum Developers
# SPDX-License-Identifier: MIT

[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string] $BuildDirectory,

  [Parameter(Mandatory = $true)]
  [ValidateSet('x64', 'arm64')]
  [string] $Architecture,

  [Parameter(Mandatory = $true)]
  [string] $CurrentTag,

  [Parameter(Mandatory = $true)]
  [string] $CurrentPackageVersion,

  [Parameter(Mandatory = $true)]
  [string] $Repository,

  [string] $DeskflowTag = 'v1.26.0'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$msiexec = Join-Path $env:SystemRoot 'System32\msiexec.exe'
$workDirectory = Join-Path $env:RUNNER_TEMP 'ieum-msi-upgrade'
New-Item -ItemType Directory -Force -Path $workDirectory | Out-Null

function Get-RelatedProducts {
  param(
    [Parameter(Mandatory = $true)]
    [string] $UpgradeCode
  )

  $instance = New-Object -ComObject WindowsInstaller.Installer
  try {
    return @($instance.RelatedProducts($UpgradeCode))
  }
  finally {
    [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($instance)
  }
}

function Get-MsiProperty {
  param(
    [Parameter(Mandatory = $true)]
    [System.IO.FileInfo] $Msi,

    [Parameter(Mandatory = $true)]
    [string] $Property
  )

  $instance = New-Object -ComObject WindowsInstaller.Installer
  try {
    $database = $instance.OpenDatabase($Msi.FullName, 0)
    try {
      $view = $database.OpenView(
        "SELECT ``Value`` FROM ``Property`` WHERE ``Property`` = '$Property'"
      )
      try {
        [void] $view.Execute()
        $record = $view.Fetch()
        if ($null -eq $record) {
          throw "$($Msi.Name) has no $Property property"
        }
        try {
          return $record.GetType().InvokeMember(
            'StringData',
            'GetProperty',
            $null,
            $record,
            @(1)
          )
        }
        finally {
          [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($record)
        }
      }
      finally {
        [void] $view.Close()
        [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($view)
      }
    }
    finally {
      [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($database)
    }
  }
  finally {
    [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($instance)
  }
}

function Get-ProductState {
  param(
    [Parameter(Mandatory = $true)]
    [string] $ProductCode
  )

  $instance = New-Object -ComObject WindowsInstaller.Installer
  try {
    return $instance.ProductState($ProductCode)
  }
  finally {
    [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($instance)
  }
}

function Get-InstalledProductInfo {
  param(
    [Parameter(Mandatory = $true)]
    [string] $ProductCode
  )

  $instance = New-Object -ComObject WindowsInstaller.Installer
  try {
    return [pscustomobject] @{
      Name = $instance.ProductInfo($ProductCode, 'ProductName')
      Version = $instance.ProductInfo($ProductCode, 'VersionString')
      InstallLocation = $instance.ProductInfo($ProductCode, 'InstallLocation')
    }
  }
  finally {
    [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($instance)
  }
}

function Write-MsiLogs {
  foreach ($log in Get-ChildItem -LiteralPath $workDirectory -Filter '*.log') {
    Write-Host "::group::Windows Installer log: $($log.FullName)"
    Get-Content -LiteralPath $log.FullName -ErrorAction SilentlyContinue
    Write-Host '::endgroup::'
  }
}

function Invoke-MsiExec {
  param(
    [Parameter(Mandatory = $true)]
    [string] $Operation,

    [Parameter(Mandatory = $true)]
    [string] $Target,

    [Parameter(Mandatory = $true)]
    [string] $Log,

    [switch] $AllowRebootRequired
  )

  $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
  $startInfo.FileName = $msiexec
  $startInfo.UseShellExecute = $false
  $startInfo.CreateNoWindow = $true
  foreach ($argument in @($Operation, $Target, '/qn', '/norestart', '/l*v', $Log)) {
    [void] $startInfo.ArgumentList.Add($argument)
  }

  $process = [System.Diagnostics.Process]::Start($startInfo)
  try {
    $process.WaitForExit()
    $exitCode = $process.ExitCode
  }
  finally {
    $process.Dispose()
  }

  if ($exitCode -eq 3010 -and $AllowRebootRequired) {
    Write-Warning "msiexec $Operation requested a reboot for an external package"
    return
  }

  if ($exitCode -ne 0) {
    Write-Host "::group::Windows Installer log: $Log"
    Get-Content -LiteralPath $Log -ErrorAction SilentlyContinue
    Write-Host '::endgroup::'
    if ($exitCode -eq 3010) {
      throw "msiexec $Operation completed but requires a reboot (exit code 3010)"
    }
    throw "msiexec $Operation failed with exit code $exitCode"
  }

  $hiddenRebootRequirement = Select-String -LiteralPath $Log -Quiet -Pattern (
      'RESTART MANAGER: Did detect that a critical application holds file\[s\] in use|' +
      'MainEngineThread is returning 3010'
    )
  if (-not $AllowRebootRequired -and $hiddenRebootRequirement) {
    Write-Host "::group::Windows Installer log: $Log"
    Get-Content -LiteralPath $Log -ErrorAction SilentlyContinue
    Write-Host '::endgroup::'
    throw "msiexec $Operation reported a hidden reboot requirement in $Log"
  }
}

function Wait-ServiceRunning {
  param(
    [Parameter(Mandatory = $true)]
    [string] $Name,

    [int] $TimeoutSeconds = 15
  )

  $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
  $service = $null
  do {
    $service = Get-Service -Name $Name -ErrorAction SilentlyContinue
    if ($null -ne $service -and $service.Status -eq 'Running') {
      return
    }
    Start-Sleep -Milliseconds 250
  } while ([DateTime]::UtcNow -lt $deadline)

  $state = if ($null -eq $service) { 'missing' } else { $service.Status }
  throw "Service '$Name' did not reach Running state (state: $state)"
}

function Test-NamedPipe {
  param(
    [Parameter(Mandatory = $true)]
    [string] $Name,

    [int] $TimeoutMilliseconds = 250
  )

  $pipe = [System.IO.Pipes.NamedPipeClientStream]::new(
    '.',
    $Name,
    [System.IO.Pipes.PipeDirection]::InOut,
    [System.IO.Pipes.PipeOptions]::None
  )
  try {
    $pipe.Connect($TimeoutMilliseconds)
    return $pipe.IsConnected
  }
  catch [TimeoutException] {
    return $false
  }
  catch [System.IO.IOException] {
    return $false
  }
  finally {
    $pipe.Dispose()
  }
}

function Wait-NamedPipe {
  param(
    [Parameter(Mandatory = $true)]
    [string] $Name,

    [int] $TimeoutSeconds = 15
  )

  $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
  do {
    if (Test-NamedPipe -Name $Name) {
      return
    }
    Start-Sleep -Milliseconds 250
  } while ([DateTime]::UtcNow -lt $deadline)

  throw "Named pipe '$Name' was not reachable within $TimeoutSeconds seconds"
}

function Read-IpcLine {
  param(
    [Parameter(Mandatory = $true)]
    [System.IO.StreamReader] $Reader,

    [Parameter(Mandatory = $true)]
    [string] $Name
  )

  $readTask = $Reader.ReadLineAsync()
  if (-not $readTask.Wait(5000)) {
    throw "Timed out waiting for a reply from named pipe '$Name'"
  }
  return $readTask.Result
}

function Invoke-IpcCommands {
  param(
    [Parameter(Mandatory = $true)]
    [string] $Name,

    [Parameter(Mandatory = $true)]
    [string[]] $Messages
  )

  $pipe = [System.IO.Pipes.NamedPipeClientStream]::new(
    '.',
    $Name,
    [System.IO.Pipes.PipeDirection]::InOut,
    [System.IO.Pipes.PipeOptions]::None
  )
  $reader = $null
  $writer = $null
  try {
    $pipe.Connect(5000)
    $encoding = [System.Text.UTF8Encoding]::new($false)
    $reader = [System.IO.StreamReader]::new($pipe, $encoding, $false, 1024, $true)
    $writer = [System.IO.StreamWriter]::new($pipe, $encoding, 1024, $true)
    $writer.AutoFlush = $true

    $writer.WriteLine('hello=ieum-runtime-coexistence-test')
    $hello = Read-IpcLine -Reader $reader -Name $Name
    if ($hello -notlike 'hello=*' -and $hello -notlike 'versionMismatch=*') {
      throw "Named pipe '$Name' returned an invalid handshake: '$hello'"
    }

    foreach ($message in $Messages) {
      $writer.WriteLine($message)
      $reply = Read-IpcLine -Reader $reader -Name $Name
      if ($reply -ne 'ok') {
        throw "Named pipe '$Name' returned '$reply' for '$message'"
      }
    }
  }
  finally {
    if ($null -ne $writer) {
      $writer.Dispose()
    }
    if ($null -ne $reader) {
      $reader.Dispose()
    }
    $pipe.Dispose()
  }
}

function New-ClientSettings {
  param(
    [Parameter(Mandatory = $true)]
    [string] $Path,

    [Parameter(Mandatory = $true)]
    [string] $ComputerName
  )

  @"
[core]
computerName=$ComputerName
coreMode=1
port=24800

[client]
remoteHost=127.0.0.1

[security]
tlsEnabled=false
checkPeerFingerprints=false
"@ | Set-Content -LiteralPath $Path -Encoding utf8NoBOM
}

function Start-ClientCore {
  param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,

    [Parameter(Mandatory = $true)]
    [string] $SettingsFile
  )

  $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
  $startInfo.FileName = $Executable
  $startInfo.UseShellExecute = $false
  $startInfo.CreateNoWindow = $true
  [void] $startInfo.ArgumentList.Add('client')
  [void] $startInfo.ArgumentList.Add('--settings')
  [void] $startInfo.ArgumentList.Add($SettingsFile)
  return [System.Diagnostics.Process]::Start($startInfo)
}

function Assert-ProcessRemainsRunning {
  param(
    [Parameter(Mandatory = $true)]
    [System.Diagnostics.Process] $Target,

    [Parameter(Mandatory = $true)]
    [string] $Description,

    [int] $ObservationMilliseconds = 2000
  )

  $deadline = [DateTime]::UtcNow.AddMilliseconds($ObservationMilliseconds)
  do {
    $Target.Refresh()
    if ($Target.HasExited) {
      throw "$Description exited unexpectedly with code $($Target.ExitCode)"
    }
    Start-Sleep -Milliseconds 250
  } while ([DateTime]::UtcNow -lt $deadline)
}

function Wait-ProcessName {
  param(
    [Parameter(Mandatory = $true)]
    [string] $Name,

    [int] $TimeoutSeconds = 15
  )

  $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
  $process = $null
  do {
    $process = Get-Process -Name $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $process) {
      return $process
    }
    Start-Sleep -Milliseconds 250
  } while ([DateTime]::UtcNow -lt $deadline)

  throw "Process '$Name' did not start within $TimeoutSeconds seconds"
}

function Wait-ProcessExit {
  param(
    [Parameter(Mandatory = $true)]
    [string] $Name,

    [int] $TimeoutSeconds = 15
  )

  $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
  do {
    if ($null -eq (Get-Process -Name $Name -ErrorAction SilentlyContinue)) {
      return
    }
    Start-Sleep -Milliseconds 250
  } while ([DateTime]::UtcNow -lt $deadline)

  throw "Process '$Name' did not stop within $TimeoutSeconds seconds"
}

function Assert-IeumServiceRuntime {
  param(
    [Parameter(Mandatory = $true)]
    [string] $InstallLocation,

    [Parameter(Mandatory = $true)]
    [string] $SettingsFile,

    [string] $CoexistingPipeName = ''
  )

  $daemon = Join-Path $InstallLocation 'ieum-daemon.exe'
  $core = Join-Path $InstallLocation 'ieum-core.exe'
  foreach ($path in @($daemon, $core)) {
    if (-not (Test-Path -LiteralPath $path)) {
      throw "Ieum runtime executable was not found: $path"
    }
  }
  foreach ($legacyName in @('deskflow-daemon.exe', 'deskflow-core.exe')) {
    $legacyPath = Join-Path $InstallLocation $legacyName
    if (Test-Path -LiteralPath $legacyPath) {
      throw "Ieum still installed Deskflow-owned runtime name: $legacyPath"
    }
  }

  Wait-ServiceRunning -Name 'Ieum'
  $service = Get-CimInstance Win32_Service -Filter "Name='Ieum'"
  if ($service.PathName -notlike '*ieum-daemon.exe*') {
    throw "Ieum service points to the wrong binary: $($service.PathName)"
  }
  Wait-NamedPipe -Name 'ieum-daemon-v1'

  $ipcSettingsPath = $SettingsFile.Replace('\', '/')
  Invoke-IpcCommands -Name 'ieum-daemon-v1' -Messages @(
    "configFile=$ipcSettingsPath",
    'start'
  )
  $primaryCore = Wait-ProcessName -Name 'ieum-core'
  Wait-NamedPipe -Name 'ieum-core-v1'
  if (-not [string]::IsNullOrWhiteSpace($CoexistingPipeName)) {
    Wait-NamedPipe -Name $CoexistingPipeName
  }

  # A GUI launched after sign-in sends the persisted service configuration
  # again. The already-running login-screen core must not be replaced.
  Invoke-IpcCommands -Name 'ieum-daemon-v1' -Messages @(
    "configFile=$ipcSettingsPath",
    'start'
  )
  Start-Sleep -Milliseconds 1500
  $primaryCore.Refresh()
  if ($primaryCore.HasExited) {
    throw 'Repeating the unchanged service configuration replaced the primary Ieum core'
  }
  $serviceCores = @(Get-Process -Name 'ieum-core' -ErrorAction SilentlyContinue)
  if ($serviceCores.Count -ne 1 -or $serviceCores[0].Id -ne $primaryCore.Id) {
    throw "Unchanged service configuration did not preserve core PID $($primaryCore.Id)"
  }

  # Simulate a reboot boundary: the daemon must restore the persisted core
  # configuration without a signed-in GUI sending another start command.
  Stop-Service -Name 'Ieum' -Force
  (Get-Service -Name 'Ieum').WaitForStatus(
    [System.ServiceProcess.ServiceControllerStatus]::Stopped,
    [TimeSpan]::FromSeconds(15)
  )
  Wait-ProcessExit -Name 'ieum-core'
  $primaryCore.Dispose()

  Start-Service -Name 'Ieum'
  Wait-ServiceRunning -Name 'Ieum'
  Wait-NamedPipe -Name 'ieum-daemon-v1'
  $primaryCore = Wait-ProcessName -Name 'ieum-core'
  Wait-NamedPipe -Name 'ieum-core-v1'
  if (-not [string]::IsNullOrWhiteSpace($CoexistingPipeName)) {
    Wait-NamedPipe -Name $CoexistingPipeName
  }

  $duplicateCore = Start-ClientCore -Executable $core -SettingsFile $SettingsFile
  try {
    if (-not $duplicateCore.WaitForExit(5000)) {
      throw 'A duplicate Ieum core was able to keep running'
    }
    if ($duplicateCore.ExitCode -eq 0) {
      throw 'A duplicate Ieum core exited successfully instead of reporting an ownership conflict'
    }
  }
  finally {
    if (-not $duplicateCore.HasExited) {
      $duplicateCore.Kill($true)
    }
    $duplicateCore.Dispose()
  }

  $primaryCore.Refresh()
  if ($primaryCore.HasExited) {
    throw 'The primary Ieum service core exited while rejecting a duplicate'
  }
  Wait-NamedPipe -Name 'ieum-core-v1'
  if (-not [string]::IsNullOrWhiteSpace($CoexistingPipeName)) {
    Wait-NamedPipe -Name $CoexistingPipeName
  }

  Invoke-IpcCommands -Name 'ieum-daemon-v1' -Messages @('stop')
  Wait-ProcessExit -Name 'ieum-core'
  $primaryCore.Dispose()
}

function Assert-UserStartupRegistration {
  param(
    [Parameter(Mandatory = $true)]
    [string] $InstallLocation
  )

  $gui = Join-Path $InstallLocation 'ieum.exe'
  $runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
  $expectedCommand = '"' + $gui + '" --background'
  $process = Start-Process -FilePath $gui -ArgumentList '--background' -PassThru
  try {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    $registeredCommand = $null
    do {
      $runRegistration = Get-ItemProperty -LiteralPath $runKey -Name 'Ieum' -ErrorAction SilentlyContinue
      $registeredCommand = if ($null -eq $runRegistration) { $null } else { $runRegistration.Ieum }
      if ($registeredCommand -eq $expectedCommand) {
        break
      }
      Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)

    if ($registeredCommand -ne $expectedCommand) {
      throw "Ieum did not register the expected startup command: $expectedCommand"
    }

    $duplicate = Start-Process -FilePath $gui -ArgumentList '--background' -PassThru
    try {
      if (-not $duplicate.WaitForExit(5000)) {
        throw 'A duplicate background GUI did not exit'
      }
      if ($duplicate.ExitCode -ne 0) {
        throw "A duplicate background GUI exited with code $($duplicate.ExitCode)"
      }
    }
    finally {
      if (-not $duplicate.HasExited) {
        $duplicate.Kill($true)
      }
      $duplicate.Dispose()
    }
  }
  finally {
    if (-not $process.HasExited) {
      $process.Kill($true)
      $process.WaitForExit()
    }
    $process.Dispose()
  }
}

$previousRevision = if ($CurrentTag -like 'v*') {
  "$CurrentTag^"
}
else {
  'HEAD'
}
$previousTag = (& git describe --tags --abbrev=0 $previousRevision).Trim()
if ([string]::IsNullOrWhiteSpace($previousTag)) {
  throw "Could not determine the release before $CurrentTag"
}

$currentMsi = Get-Item -LiteralPath (
  Join-Path $BuildDirectory "Ieum-$CurrentPackageVersion-win-$Architecture-ko-KR.msi"
)
$previousPattern = "Ieum-$($previousTag.TrimStart('v'))-win-$Architecture-ko-KR.msi"

Write-Host "Downloading $previousPattern for an upgrade test"
& gh release download $previousTag `
  --repo $Repository `
  --dir $workDirectory `
  --clobber `
  --pattern $previousPattern
if ($LASTEXITCODE -ne 0) {
  throw "Failed to download $previousPattern"
}
$previousMsi = Get-Item -LiteralPath (Join-Path $workDirectory $previousPattern)
$previousProductCode = Get-MsiProperty -Msi $previousMsi -Property 'ProductCode'
$currentProductCode = Get-MsiProperty -Msi $currentMsi -Property 'ProductCode'
$previousProductVersion = [version] (Get-MsiProperty -Msi $previousMsi -Property 'ProductVersion')
$currentProductVersion = [version] (Get-MsiProperty -Msi $currentMsi -Property 'ProductVersion')
$previousUpgradeCode = Get-MsiProperty -Msi $previousMsi -Property 'UpgradeCode'
$currentUpgradeCode = Get-MsiProperty -Msi $currentMsi -Property 'UpgradeCode'
$upgradeCodes = @($previousUpgradeCode, $currentUpgradeCode) | Sort-Object -Unique

if ($currentProductCode -eq $previousProductCode) {
  throw 'The release reused the previous MSI ProductCode'
}
if ($currentProductVersion -le $previousProductVersion) {
  throw "Current MSI version $currentProductVersion is not newer than $previousProductVersion"
}

try {
  foreach ($upgradeCode in $upgradeCodes) {
    $existingProducts = @(Get-RelatedProducts -UpgradeCode $upgradeCode)
    if ($existingProducts.Count -ne 0) {
      throw "The CI runner already has a product registered with UpgradeCode $upgradeCode"
    }
  }

  Write-Host "Installing previous package $($previousMsi.Name)"
  Invoke-MsiExec `
    -Operation '/i' `
    -Target $previousMsi.FullName `
    -Log (Join-Path $workDirectory 'previous-install.log')

  $previousState = Get-ProductState -ProductCode $previousProductCode
  $previousProducts = @(Get-RelatedProducts -UpgradeCode $previousUpgradeCode)
  if ($previousState -ne 5) {
    throw "Previous ProductCode state is $previousState, expected 5 (installed)"
  }
  if ($previousProducts.Count -ne 1 -or $previousProducts[0] -ne $previousProductCode) {
    throw "Expected only $previousProductCode after installing $previousTag; found $($previousProducts -join ', ')"
  }
  Write-Host "  Previous ProductCode=$previousProductCode"

  Write-Host "Upgrading with $($currentMsi.Name)"
  Invoke-MsiExec `
    -Operation '/i' `
    -Target $currentMsi.FullName `
    -Log (Join-Path $workDirectory 'current-install.log')

  $previousState = Get-ProductState -ProductCode $previousProductCode
  $currentState = Get-ProductState -ProductCode $currentProductCode
  $currentProducts = @(Get-RelatedProducts -UpgradeCode $currentUpgradeCode)
  if ($previousState -ne -1) {
    throw "Upgrade left previous ProductCode $previousProductCode in state $previousState"
  }
  if ($currentState -ne 5) {
    throw "Current ProductCode state is $currentState, expected 5 (installed)"
  }
  if ($currentProducts.Count -ne 1 -or $currentProducts[0] -ne $currentProductCode) {
    throw "Expected only $currentProductCode after upgrade; found $($currentProducts -join ', ')"
  }
  if ($previousUpgradeCode -ne $currentUpgradeCode) {
    $legacyProducts = @(Get-RelatedProducts -UpgradeCode $previousUpgradeCode)
    if ($legacyProducts.Count -ne 0) {
      throw "Upgrade left products registered under legacy UpgradeCode $previousUpgradeCode"
    }
  }

  $product = Get-InstalledProductInfo -ProductCode $currentProductCode
  if ($product.Name -ne '이음 (Ieum)') {
    throw "Installed apps name is '$($product.Name)', expected '이음 (Ieum)'"
  }
  if ([string]::IsNullOrWhiteSpace($product.InstallLocation)) {
    throw 'Windows Installer did not register InstallLocation'
  }

  $core = Join-Path $product.InstallLocation 'ieum-core.exe'
  if (-not (Test-Path -LiteralPath $core)) {
    throw "Installed core executable was not found: $core"
  }
  & $core --version
  if ($LASTEXITCODE -ne 0) {
    throw "Installed core executable failed with exit code $LASTEXITCODE"
  }

  Assert-UserStartupRegistration -InstallLocation $product.InstallLocation

  $runtimeSettings = Join-Path $workDirectory 'ieum-upgrade-runtime.conf'
  New-ClientSettings -Path $runtimeSettings -ComputerName 'ieum-upgrade-ci'
  Assert-IeumServiceRuntime -InstallLocation $product.InstallLocation -SettingsFile $runtimeSettings

  Write-Host (
    "Upgrade and service runtime passed: {0} {1}, ProductCode={2}" -f
    $product.Name,
    $product.Version,
    $currentProductCode
  )
  Write-Host "  UpgradeCode: $previousUpgradeCode -> $currentUpgradeCode"
}
catch {
  Write-MsiLogs
  throw
}
finally {
  foreach ($productCode in @($currentProductCode, $previousProductCode)) {
    if ((Get-ProductState -ProductCode $productCode) -ne -1) {
      Write-Host "Removing test product $productCode"
      Invoke-MsiExec `
        -Operation '/x' `
        -Target $productCode `
        -Log (Join-Path $workDirectory "uninstall-$($productCode.Trim('{}')).log")
    }
  }
  $runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
  $runRegistration = Get-ItemProperty -LiteralPath $runKey -Name 'Ieum' -ErrorAction SilentlyContinue
  $startupValue = if ($null -eq $runRegistration) { $null } else { $runRegistration.Ieum }
  if ($null -ne $startupValue) {
    throw "Uninstall left the Ieum startup registration behind: $startupValue"
  }
}

$deskflowVersion = $DeskflowTag.TrimStart('v')
$deskflowPattern = "deskflow-$deskflowVersion-win-$Architecture.msi"
Write-Host "Downloading $deskflowPattern for the coexistence test"
& gh release download $DeskflowTag `
  --repo 'deskflow/deskflow' `
  --dir $workDirectory `
  --clobber `
  --pattern $deskflowPattern
if ($LASTEXITCODE -ne 0) {
  throw "Failed to download $deskflowPattern"
}

$deskflowMsi = Get-Item -LiteralPath (Join-Path $workDirectory $deskflowPattern)
$deskflowProductCode = Get-MsiProperty -Msi $deskflowMsi -Property 'ProductCode'
$deskflowUpgradeCode = Get-MsiProperty -Msi $deskflowMsi -Property 'UpgradeCode'
$legacyDeskflowUpgradeCode = '{027D1C8A-E7A5-4754-BB93-B2D45BFDBDC8}'
if ($deskflowUpgradeCode -ne $legacyDeskflowUpgradeCode) {
  throw "Deskflow UpgradeCode '$deskflowUpgradeCode' no longer matches the historical collision identity"
}
if ($deskflowUpgradeCode -eq $currentUpgradeCode) {
  throw "Ieum still shares Deskflow's UpgradeCode"
}

$deskflowCoreProcess = $null
try {
  Write-Host "Installing upstream package $($deskflowMsi.Name)"
  Invoke-MsiExec `
    -Operation '/i' `
    -Target $deskflowMsi.FullName `
    -Log (Join-Path $workDirectory 'deskflow-install.log') `
    -AllowRebootRequired

  if ((Get-ProductState -ProductCode $deskflowProductCode) -ne 5) {
    throw "Deskflow did not reach installed state"
  }

  $deskflowProduct = Get-InstalledProductInfo -ProductCode $deskflowProductCode
  Wait-ServiceRunning -Name 'Deskflow'
  Wait-NamedPipe -Name 'deskflow-daemon'

  $deskflowCore = Join-Path $deskflowProduct.InstallLocation 'deskflow-core.exe'
  if (-not (Test-Path -LiteralPath $deskflowCore)) {
    throw "Deskflow core executable was not found: $deskflowCore"
  }
  $deskflowSettings = Join-Path $workDirectory 'deskflow-coexist-runtime.conf'
  New-ClientSettings -Path $deskflowSettings -ComputerName 'deskflow-coexist-ci'
  $deskflowCoreProcess = Start-ClientCore -Executable $deskflowCore -SettingsFile $deskflowSettings
  # Deskflow 1.26 exposes daemon IPC but predates core IPC. Process liveness
  # proves that Ieum's renamed executable and shared-memory identity coexist.
  Assert-ProcessRemainsRunning -Target $deskflowCoreProcess -Description 'Deskflow core during startup'

  Write-Host "Installing Ieum alongside Deskflow"
  Invoke-MsiExec `
    -Operation '/i' `
    -Target $currentMsi.FullName `
    -Log (Join-Path $workDirectory 'ieum-coexist-install.log')

  $deskflowState = Get-ProductState -ProductCode $deskflowProductCode
  $currentState = Get-ProductState -ProductCode $currentProductCode
  if ($deskflowState -ne 5 -or $currentState -ne 5) {
    throw "Coexistence failed: Deskflow state=$deskflowState, Ieum state=$currentState"
  }

  $deskflowProducts = @(Get-RelatedProducts -UpgradeCode $deskflowUpgradeCode)
  $ieumProducts = @(Get-RelatedProducts -UpgradeCode $currentUpgradeCode)
  if ($deskflowProducts.Count -ne 1 -or $deskflowProducts[0] -ne $deskflowProductCode) {
    throw "Ieum changed Deskflow's product registration"
  }
  if ($ieumProducts.Count -ne 1 -or $ieumProducts[0] -ne $currentProductCode) {
    throw "Ieum was not registered under its independent UpgradeCode"
  }

  $ieumProduct = Get-InstalledProductInfo -ProductCode $currentProductCode
  if ($deskflowProduct.Name -ne 'Deskflow' -or $ieumProduct.Name -ne '이음 (Ieum)') {
    throw "Unexpected installed names: '$($deskflowProduct.Name)', '$($ieumProduct.Name)'"
  }

  Assert-ProcessRemainsRunning `
    -Target $deskflowCoreProcess `
    -Description 'Deskflow core after installing Ieum' `
    -ObservationMilliseconds 1000
  Wait-NamedPipe -Name 'deskflow-daemon'

  $ieumSettings = Join-Path $workDirectory 'ieum-coexist-runtime.conf'
  New-ClientSettings -Path $ieumSettings -ComputerName 'ieum-coexist-ci'
  Assert-IeumServiceRuntime `
    -InstallLocation $ieumProduct.InstallLocation `
    -SettingsFile $ieumSettings `
    -CoexistingPipeName 'deskflow-daemon'

  Assert-ProcessRemainsRunning `
    -Target $deskflowCoreProcess `
    -Description 'Deskflow core after starting the Ieum service core' `
    -ObservationMilliseconds 1000
  Wait-NamedPipe -Name 'deskflow-daemon'

  Write-Host (
    "Install and runtime coexistence passed: {0} {1} and {2} {3}" -f
    $deskflowProduct.Name,
    $deskflowProduct.Version,
    $ieumProduct.Name,
    $ieumProduct.Version
  )
}
catch {
  Write-MsiLogs
  throw
}
finally {
  if ($null -ne $deskflowCoreProcess) {
    $deskflowCoreProcess.Refresh()
    if (-not $deskflowCoreProcess.HasExited) {
      $deskflowCoreProcess.Kill($true)
      [void] $deskflowCoreProcess.WaitForExit(5000)
    }
    $deskflowCoreProcess.Dispose()
  }

  foreach ($productCode in @($currentProductCode, $deskflowProductCode)) {
    if ((Get-ProductState -ProductCode $productCode) -ne -1) {
      Write-Host "Removing coexistence test product $productCode"
      Invoke-MsiExec `
        -Operation '/x' `
        -Target $productCode `
        -Log (Join-Path $workDirectory "uninstall-$($productCode.Trim('{}')).log") `
        -AllowRebootRequired:($productCode -eq $deskflowProductCode)
    }
  }
}
