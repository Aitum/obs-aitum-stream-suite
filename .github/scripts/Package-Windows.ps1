[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function PackagePlugin {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
        )
    }

    Remove-Item @RemoveArgs

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.exe"
        )
    }

    Remove-Item @RemoveArgs

    $IsccFile = "${ProjectRoot}/build_${Target}/installer-Windows.generated.iss"

    if ( ! ( Test-Path -Path $IsccFile ) ) {
        throw 'InnoSetup install script not found. Run the build script or the CMake build and install procedures first.'
    }

    Copy-Item -Path "${ProjectRoot}/release/${Configuration}/${ProductName}/bin" -Destination "${ProjectRoot}/release/Package/obs-plugins" -Recurse
    Copy-Item -Path "${ProjectRoot}/release/${Configuration}/${ProductName}/data" -Destination "${ProjectRoot}/release/Package/data/obs-plugins/${ProductName}" -Recurse

    $plugins = @('advanced-masks.1856','move.913','retro-effects.1972','composite-blur.1780','svg-source.2174','noise.1916','stroke-glow-shadow.1800','freeze-filter.950','3d-effect.1692','scene-notes-dock.1398','source-clone.1632','gradient-source.1172')
    foreach ($plugin in $plugins) {
        $content = (Invoke-WebRequest "https://obsproject.com/forum/resources/${plugin}/download").Content -replace "\s",""
        $legacyPath = [regex]::Match($content, '<ahref="(/forum/resources/[^"]+)"((?!forum/resources).)*windows(-legacy|.OBS30-32)(-x64)?\.zip').Groups[1].Value
        if ($legacyPath) {
            $relPath = $legacyPath
        }else {
            $relPath = [regex]::Match($content, '<ahref="(/forum/resources/[^"]+)"((?!forum/resources).)*windows(-x64)?\.zip').Groups[1].Value
        }
        if ($relPath) {
            Invoke-WebRequest -OutFile "temp.zip" "https://obsproject.com${relPath}"
            $shortName = $plugin.Split('.')[0]
            Expand-Archive -Path "temp.zip" -DestinationPath "${ProjectRoot}/${shortName}/" -Force
            Remove-Item -Path "temp.zip"
            if ((Test-Path "${ProjectRoot}/${shortName}/obs-plugins") -and (Test-Path "${ProjectRoot}/${shortName}/data")) {
                Copy-Item -Path "${ProjectRoot}/${shortName}/obs-plugins" -Destination "${ProjectRoot}/release/Package" -Recurse -Force
                Copy-Item -Path "${ProjectRoot}/${shortName}/data" -Destination "${ProjectRoot}/release/Package" -Recurse -Force
            }else{
                $dllPaths = Get-ChildItem -Path "${ProjectRoot}/${shortName}/" -Recurse -Filter "*.dll"
                if ($dllPaths) {
                    foreach ($dllPath in $dllPaths) {
                        $dirName = Split-Path -Path $dllPath.FullName -Parent
                        if(Test-Path -Path "$dirName/../../data"){
                            Copy-Item -Path "$dirName" -Destination "${ProjectRoot}/release/Package/obs-plugins" -Recurse -Force
                            Copy-Item -Path "$dirName/../../data" -Destination "${ProjectRoot}/release/Package/" -Recurse -Force
                        }                       
                    }
                }else{
                    Write-Warning "No obs-plugins or data folder found for ${plugin}"
                }
            }
            Remove-Item -Path "${ProjectRoot}/${shortName}/" -Recurse -Force
        } else {
            Write-Warning "No Windows download found for ${plugin}"
        }
    }

    Copy-Item -Path "${ProjectRoot}/theme" -Destination "${ProjectRoot}/release/Package/data/obs-studio/themes" -Recurse
    Copy-Item "${IsccFile}" -Destination "${ProjectRoot}/release"
    Copy-Item "${ProjectRoot}/media/icon.ico" -Destination "${ProjectRoot}/release"

    Log-Information 'Creating InnoSetup installer...'
    Push-Location -Stack BuildTemp
    Ensure-Location -Path "${ProjectRoot}/release"
    Invoke-External iscc ${IsccFile} /O. /F"${OutputName}-Installer"
    Pop-Location -Stack BuildTemp

    Log-Group "Archiving ${ProductName}..."
    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release/${Configuration}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}-programdata.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs

    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release/Package" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs

    Log-Group
}

PackagePlugin
