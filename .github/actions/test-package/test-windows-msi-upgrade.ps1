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
  [string] $Repository
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$upgradeCode = '{027D1C8A-E7A5-4754-BB93-B2D45BFDBDC8}'
$msiexec = Join-Path $env:SystemRoot 'System32\msiexec.exe'
$workDirectory = Join-Path $env:RUNNER_TEMP 'ieum-msi-upgrade'
New-Item -ItemType Directory -Force -Path $workDirectory | Out-Null

function Get-RelatedProducts {
  param([__ComObject] $Installer)

  return @($Installer.RelatedProducts($upgradeCode))
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

  & $msiexec $Operation $Target /qn /norestart /l*v $Log
  $exitCode = $LASTEXITCODE
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

$installer = New-Object -ComObject WindowsInstaller.Installer
try {
  $existingProducts = @(Get-RelatedProducts -Installer $installer)
  if ($existingProducts.Count -ne 0) {
    throw "The CI runner already has a product registered with the Ieum UpgradeCode"
  }

  try {
    Write-Host "Installing previous package $($previousMsi.Name)"
    Invoke-MsiExec `
      -Operation '/i' `
      -Target $previousMsi.FullName `
      -Log (Join-Path $workDirectory 'previous-install.log')

    $previousProducts = @(Get-RelatedProducts -Installer $installer)
    if ($previousProducts.Count -ne 1) {
      throw "Expected one Ieum product after installing $previousTag, found $($previousProducts.Count)"
    }
    $previousProductCode = $previousProducts[0]
    Write-Host "  Previous ProductCode=$previousProductCode"

    Write-Host "Upgrading with $($currentMsi.Name)"
    Invoke-MsiExec `
      -Operation '/i' `
      -Target $currentMsi.FullName `
      -Log (Join-Path $workDirectory 'current-install.log')

    $currentProducts = @(Get-RelatedProducts -Installer $installer)
    if ($currentProducts.Count -ne 1) {
      throw "Expected exactly one Ieum registration after upgrade, found $($currentProducts.Count)"
    }

    $currentProductCode = $currentProducts[0]
    if ($currentProductCode -eq $previousProductCode) {
      throw 'The release did not replace the previous MSI ProductCode'
    }

    $productName = $installer.ProductInfo($currentProductCode, 'ProductName')
    $productVersion = $installer.ProductInfo($currentProductCode, 'VersionString')
    $installLocation = $installer.ProductInfo($currentProductCode, 'InstallLocation')
    if ($productName -ne '이음 (Ieum)') {
      throw "Installed apps name is '$productName', expected '이음 (Ieum)'"
    }
    if ([string]::IsNullOrWhiteSpace($installLocation)) {
      throw 'Windows Installer did not register InstallLocation'
    }

    $core = Join-Path $installLocation 'deskflow-core.exe'
    if (-not (Test-Path -LiteralPath $core)) {
      throw "Installed core executable was not found: $core"
    }
    & $core --version
    if ($LASTEXITCODE -ne 0) {
      throw "Installed core executable failed with exit code $LASTEXITCODE"
    }

    Write-Host (
      "Upgrade passed: {0} {1}, ProductCode={2}" -f
      $productName,
      $productVersion,
      $currentProductCode
    )
  }
  finally {
    foreach ($productCode in @(Get-RelatedProducts -Installer $installer)) {
      Write-Host "Removing test product $productCode"
      Invoke-MsiExec `
        -Operation '/x' `
        -Target $productCode `
        -Log (Join-Path $workDirectory "uninstall-$($productCode.Trim('{}')).log")
    }
  }
}
finally {
  [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($installer)
}
