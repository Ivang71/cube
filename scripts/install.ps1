$ErrorActionPreference = "Stop"

Set-Location (Join-Path $PSScriptRoot "..")

$packages = @(
    @{ Id = "Kitware.CMake"; Name = "CMake" }
    @{ Id = "Git.Git"; Name = "Git" }
    @{ Id = "LunarG.VulkanSDK"; Name = "Vulkan SDK" }
    @{ Id = "LLVM.LLVM"; Name = "LLVM/Clang" }
    @{ Id = "Ninja-build.Ninja"; Name = "Ninja" }
)

foreach ($p in $packages) {
    Write-Host "Installing $($p.Name)..."
    winget install --id $($p.Id) --source winget -e --accept-package-agreements --accept-source-agreements
}






