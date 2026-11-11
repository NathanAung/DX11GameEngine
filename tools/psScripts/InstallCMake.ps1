
$cmakeDownloadUrl = "https://github.com/Kitware/CMake/releases/download/v4.2.1/cmake-4.2.1-windows-x86_64.zip"


Write-Output "[CMakeInstall] Downloading w64cmake..."
curl.exe -L "$cmakeDownloadUrl" -o "cmake.zip" --progress-bar

Write-Output "[CMakeInstall] Extracting w64cmake in $($env:LOCALAPPDATA)..."
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory("$($PSScriptRoot)\cmake.zip", "$($env:LOCALAPPDATA)")

Rename-Item -Path "$($env:LOCALAPPDATA)\cmake-4.2.1-windows-x86_64" -NewName "w64cmake"

Remove-Item -Path "$($PSScriptRoot)\cmake.zip" -Force

return 0
