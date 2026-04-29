param(
    [string]$Distro = "Ubuntu",
    [string]$VidPid = "04b4:f155"
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command usbipd -ErrorAction SilentlyContinue)) {
    throw "usbipd is not available in PATH."
}

$listOutput = usbipd list
$targetLine = $listOutput | Where-Object { $_ -match $VidPid } | Select-Object -First 1

if (-not $targetLine) {
    throw "Device with VID:PID $VidPid not found. Check 'usbipd list'."
}

# BUSID is first token on the line (example: 1-3)
$busId = ($targetLine -split "\s+")[0]

if (-not $busId) {
    throw "Unable to parse BUSID from: $targetLine"
}

Write-Host "Using BUSID: $busId (VID:PID $VidPid)"

# Try binding first (safe if already shared/bound)
try {
    usbipd bind --busid $busId | Out-Host
} catch {
    Write-Host "Bind step skipped/failed, continuing to attach..."
}

usbipd attach --wsl --distribution $Distro --busid $busId | Out-Host
Write-Host "Attach complete. Verify in WSL with: lsusb"
