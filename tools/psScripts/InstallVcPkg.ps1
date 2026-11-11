
git clone "https://github.com/microsoft/vcpkg.git" "$($env:SystemDrive)\vcpkg"

Write-Output "[CMakeInstall] Bootstrapping vcpkg..."
& "$($env:SystemDrive)\vcpkg\bootstrap-vcpkg.bat"

return 0
