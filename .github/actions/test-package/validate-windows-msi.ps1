# SPDX-FileCopyrightText: (C) 2026 Ieum Developers
# SPDX-License-Identifier: MIT

[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string] $BuildDirectory,

  [Parameter(Mandatory = $true)]
  [string] $ReleaseVersion
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$upgradeCode = '{027D1C8A-E7A5-4754-BB93-B2D45BFDBDC8}'

function ConvertTo-WindowsInstallerVersion {
  param([string] $Version)

  $channelOffsets = @{
    alpha = 100
    beta = 400
    rc = 700
  }

  if ($Version -match '^(\d+)\.(\d+)\.(\d+)-(alpha|beta|rc)\.(\d+)$') {
    $sequence = [int] $Matches[5]
    if ($sequence -gt 199) {
      throw "Prerelease sequence exceeds 199: $Version"
    }

    $build = ([int] $Matches[3] * 1000) + $channelOffsets[$Matches[4]] + $sequence
    return "$($Matches[1]).$($Matches[2]).$build"
  }

  if ($Version -match '^(\d+)\.(\d+)\.(\d+)$') {
    $build = ([int] $Matches[3] * 1000) + 999
    return "$($Matches[1]).$($Matches[2]).$build"
  }

  if ($Version -eq 'continuous') {
    return $null
  }

  throw "Unsupported release version: $Version"
}

function Get-MsiRows {
  param(
    [Parameter(Mandatory = $true)]
    [__ComObject] $Database,

    [Parameter(Mandatory = $true)]
    [string] $Query
  )

  $view = $Database.OpenView($Query)
  try {
    [void] $view.Execute()
    while ($record = $view.Fetch()) {
      $fieldCount = $record.GetType().InvokeMember(
        'FieldCount',
        'GetProperty',
        $null,
        $record,
        $null
      )
      $row = for ($index = 1; $index -le $fieldCount; $index++) {
        $record.GetType().InvokeMember(
          'StringData',
          'GetProperty',
          $null,
          $record,
          @($index)
        )
      }
      [pscustomobject] @{
        Values = [object[]] $row
      }
    }
  }
  finally {
    [void] $view.Close()
    [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($view)
  }
}

$msiFiles = @(
  Get-ChildItem -LiteralPath $BuildDirectory -Filter 'Ieum-*-win-*.msi' |
    Sort-Object Name
)
if ($msiFiles.Count -ne 2) {
  throw "Expected the global and ko-KR MSI packages, found $($msiFiles.Count) in $BuildDirectory"
}

$expectedVersion = ConvertTo-WindowsInstallerVersion -Version $ReleaseVersion
$installer = New-Object -ComObject WindowsInstaller.Installer

try {
  foreach ($msi in $msiFiles) {
    Write-Host "Validating $($msi.Name)"
    $database = $installer.OpenDatabase($msi.FullName, 0)
    try {
      $properties = @{}
      foreach ($row in Get-MsiRows -Database $database -Query 'SELECT `Property`,`Value` FROM `Property`') {
        $properties[$row.Values[0]] = $row.Values[1]
      }

      $expectedName = if ($msi.Name -like '*-ko-KR.msi') {
        '이음 (Ieum)'
      }
      else {
        'Ieum'
      }

      if ($properties.ProductName -ne $expectedName) {
        throw "$($msi.Name): ProductName '$($properties.ProductName)' != '$expectedName'"
      }
      if ($properties.UpgradeCode -ne $upgradeCode) {
        throw "$($msi.Name): unexpected UpgradeCode '$($properties.UpgradeCode)'"
      }
      if ($properties.ContainsKey('ARPSYSTEMCOMPONENT') -and $properties.ARPSYSTEMCOMPONENT -ne '0') {
        throw "$($msi.Name): ARPSYSTEMCOMPONENT hides Ieum from Installed apps"
      }
      foreach ($property in @('ARPCOMMENTS', 'ARPHELPLINK', 'ARPURLINFOABOUT', 'ARPURLUPDATEINFO', 'ARPPRODUCTICON')) {
        if ([string]::IsNullOrWhiteSpace($properties[$property])) {
          throw "$($msi.Name): missing Add/Remove Programs property $property"
        }
      }

      $runIeumRows = @(
        Get-MsiRows -Database $database -Query (
          "SELECT ``Action``,``Target`` FROM ``CustomAction`` WHERE ``Action`` = 'RunIeum'"
        )
      )
      if ($runIeumRows.Count -ne 1 -or $runIeumRows[0].Values[1] -notmatch '(?:^|\s)--show(?:\s|$)') {
        throw "$($msi.Name): the post-install launch does not force the main window to show"
      }

      if ($properties.ProductVersion -notmatch '^\d+\.\d+\.\d+$') {
        throw "$($msi.Name): ProductVersion must have three numeric fields, got '$($properties.ProductVersion)'"
      }
      if ($null -ne $expectedVersion -and $properties.ProductVersion -ne $expectedVersion) {
        throw "$($msi.Name): ProductVersion '$($properties.ProductVersion)' != '$expectedVersion'"
      }
      if ($ReleaseVersion -eq 'continuous' -and [int] ($properties.ProductVersion -split '\.')[2] -le 0) {
        throw "$($msi.Name): continuous ProductVersion must be newer than legacy 0.1.0.0 packages"
      }

      $upgradeRows = @(
        Get-MsiRows -Database $database -Query (
          'SELECT `VersionMin`,`VersionMax`,`Attributes`,`ActionProperty` FROM `Upgrade`'
        )
      )
      $upgradeRow = @(
        $upgradeRows | Where-Object { $_.Values[3] -eq 'WIX_UPGRADE_DETECTED' }
      )
      $downgradeRow = @(
        $upgradeRows | Where-Object { $_.Values[3] -eq 'WIX_DOWNGRADE_DETECTED' }
      )
      if ($upgradeRow.Count -ne 1 -or $downgradeRow.Count -ne 1) {
        throw "$($msi.Name): expected one upgrade and one downgrade detection row"
      }
      if ($upgradeRow[0].Values[1] -ne $properties.ProductVersion) {
        throw "$($msi.Name): upgrade VersionMax does not match ProductVersion"
      }
      if (([int] $upgradeRow[0].Values[2] -band 512) -eq 0) {
        throw "$($msi.Name): same-version replacement is not enabled"
      }
      if ($downgradeRow[0].Values[0] -ne $properties.ProductVersion) {
        throw "$($msi.Name): downgrade VersionMin does not match ProductVersion"
      }

      Write-Host (
        "  ProductName={0}; ProductVersion={1}; ProductCode={2}" -f
        $properties.ProductName,
        $properties.ProductVersion,
        $properties.ProductCode
      )
    }
    finally {
      [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($database)
    }
  }
}
finally {
  [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($installer)
}
