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
    [string] $Log
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

  if ($exitCode -notin @(0, 3010)) {
    Write-Host "::group::Windows Installer log: $Log"
    Get-Content -LiteralPath $Log -ErrorAction SilentlyContinue
    Write-Host '::endgroup::'
    throw "msiexec $Operation failed with exit code $exitCode"
  }
}

$previousTag = (& git describe --tags --abbrev=0 "$CurrentTag^").Trim()
if ([string]::IsNullOrWhiteSpace($previousTag)) {
  throw "Could not determine the release before $CurrentTag"
}

$currentMsi = Get-Item -LiteralPath (
  Join-Path $BuildDirectory "Ieum-$($CurrentTag.TrimStart('v'))-win-$Architecture-ko-KR.msi"
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
$previousUpgradeCode = Get-MsiProperty -Msi $previousMsi -Property 'UpgradeCode'
$currentUpgradeCode = Get-MsiProperty -Msi $currentMsi -Property 'UpgradeCode'
$upgradeCodes = @($previousUpgradeCode, $currentUpgradeCode) | Sort-Object -Unique

if ($currentProductCode -eq $previousProductCode) {
  throw 'The release reused the previous MSI ProductCode'
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

  $core = Join-Path $product.InstallLocation 'deskflow-core.exe'
  if (-not (Test-Path -LiteralPath $core)) {
    throw "Installed core executable was not found: $core"
  }
  & $core --version
  if ($LASTEXITCODE -ne 0) {
    throw "Installed core executable failed with exit code $LASTEXITCODE"
  }

  Write-Host (
    "Upgrade passed: {0} {1}, ProductCode={2}" -f
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
if ($deskflowUpgradeCode -ne $previousUpgradeCode) {
  throw "Deskflow no longer uses the legacy UpgradeCode required for this regression test"
}
if ($deskflowUpgradeCode -eq $currentUpgradeCode) {
  throw "Ieum still shares Deskflow's UpgradeCode"
}

try {
  Write-Host "Installing upstream package $($deskflowMsi.Name)"
  Invoke-MsiExec `
    -Operation '/i' `
    -Target $deskflowMsi.FullName `
    -Log (Join-Path $workDirectory 'deskflow-install.log')

  if ((Get-ProductState -ProductCode $deskflowProductCode) -ne 5) {
    throw "Deskflow did not reach installed state"
  }

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

  $deskflowProduct = Get-InstalledProductInfo -ProductCode $deskflowProductCode
  $ieumProduct = Get-InstalledProductInfo -ProductCode $currentProductCode
  if ($deskflowProduct.Name -ne 'Deskflow' -or $ieumProduct.Name -ne '이음 (Ieum)') {
    throw "Unexpected installed names: '$($deskflowProduct.Name)', '$($ieumProduct.Name)'"
  }

  Write-Host (
    "Coexistence passed: {0} {1} and {2} {3}" -f
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
  foreach ($productCode in @($currentProductCode, $deskflowProductCode)) {
    if ((Get-ProductState -ProductCode $productCode) -ne -1) {
      Write-Host "Removing coexistence test product $productCode"
      Invoke-MsiExec `
        -Operation '/x' `
        -Target $productCode `
        -Log (Join-Path $workDirectory "uninstall-$($productCode.Trim('{}')).log")
    }
  }
}
