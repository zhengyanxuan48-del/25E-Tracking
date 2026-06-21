param(
    [string]$JLinkPath = $env:JLINK_EXE,
    [string]$CommandFile = "tools/flash_mspm0g3507.jlink",
    [string]$HexFile = "Debug/LED1.hex",
    [string]$MakeExe = "D:\TI-MPSM0\ti\ccs2050\ccs\utils\bin\gmake.exe",
    [switch]$Build,
    [switch]$CheckOnly
)

$ErrorActionPreference = "Stop"

$candidatePaths = @()
if ($JLinkPath) {
    $candidatePaths += $JLinkPath
}
if ($env:ProgramFiles) {
    $candidatePaths += (Join-Path $env:ProgramFiles "SEGGER\JLink\JLink.exe")
}
if (${env:ProgramFiles(x86)}) {
    $candidatePaths += (Join-Path ${env:ProgramFiles(x86)} "SEGGER\JLink\JLink.exe")
}
$candidatePaths += "C:\SEGGER\JLink\JLink.exe"
$candidatePaths += (
    "D:\24{0}{1}h{2}\25E\New Folder\JLink_V950\JLink.exe" -f
    [char]0x7535, [char]0x8D5B, [char]0x9898)

$resolvedJLink = $null
foreach ($path in $candidatePaths) {
    if ($path -and (Test-Path -LiteralPath $path)) {
        $resolvedJLink = (Resolve-Path -LiteralPath $path).Path
        break
    }
}

if (-not $resolvedJLink) {
    Write-Host "[JLINK] ERROR: JLink.exe not found. Install SEGGER J-Link Software or set JLINK_EXE to the full JLink.exe path."
    exit 2
}

if (-not (Test-Path -LiteralPath $CommandFile)) {
    Write-Host "[JLINK] ERROR: J-Link command file not found: $CommandFile"
    exit 3
}

if ($Build) {
    if (-not (Test-Path -LiteralPath $MakeExe)) {
        Write-Host "[BUILD] ERROR: gmake.exe not found: $MakeExe"
        exit 5
    }

    Write-Host "[BUILD] exe=$MakeExe"
    Write-Host "[BUILD] target=all"
    & $MakeExe -f makefile all SHELL=cmd.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[BUILD] ERROR: build failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path -LiteralPath $HexFile)) {
    Write-Host "[JLINK] ERROR: HEX file not found: $HexFile. Build the project first."
    exit 4
}

Write-Host "[JLINK] exe=$resolvedJLink"
Write-Host "[JLINK] cmd=$CommandFile"
Write-Host "[JLINK] hex=$HexFile"

if ($CheckOnly) {
    Write-Host "[JLINK] check only, no flash executed."
    exit 0
}

$jlinkOutput = & $resolvedJLink -NoGui 1 -CommandFile $CommandFile 2>&1
$jlinkExitCode = $LASTEXITCODE
$jlinkText = ($jlinkOutput | Out-String)

$jlinkOutput | ForEach-Object {
    Write-Host $_
}

if ($jlinkExitCode -ne 0) {
    Write-Host "[JLINK] ERROR: JLink.exe exited with code $jlinkExitCode"
    exit $jlinkExitCode
}

if ($jlinkText -match "TRES=0" -and
    ($jlinkText -match "Failed to initialize DAP" -or
     $jlinkText -match "Could not connect to the target device" -or
     $jlinkText -match "No AHB-AP")) {
    Write-Host "[JLINK] ERROR: target RESET appears to be held low (ShowHWStatus: TRES=0). Release the reset button and check the RESET/nRST wiring before flashing."
    exit 8
}

if ($jlinkText -match "TRES=1" -and
    $jlinkText -match "No AHB-AP") {
    Write-Host "[JLINK] ERROR: target RESET is released (TRES=1), but SWD could not find the Cortex-M AHB-AP. Check SWDIO=PA19, SWCLK=PA20, GND, target power stability, and whether another load is pulling the MCU down."
    exit 9
}

if ($jlinkText -match "FAILED: Cannot connect to the probe/programmer" -or
    $jlinkText -match "Cannot connect to target" -or
    $jlinkText -match "Could not connect" -or
    $jlinkText -match "Failed to initialize DAP" -or
    $jlinkText -match "No AHB-AP" -or
    $jlinkText -match "Download failed") {
    Write-Host "[JLINK] ERROR: flash failed according to J-Link output."
    exit 6
}

if ($jlinkText -notmatch "O\.K\.") {
    Write-Host "[JLINK] ERROR: J-Link output did not contain the expected O.K. flash confirmation."
    exit 7
}

Write-Host "[JLINK] flash success."
exit 0
