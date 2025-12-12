$root = Resolve-Path "$PSScriptRoot/.."
if ($args.Count -gt 0) {
    $config = $args[0].ToLowerInvariant()
} else {
    $config = "debug"
}

switch ($config) {
    "debug" {
        $preset = "debug"
        $exe = "$root/build/debug/Debug/cube.exe"
    }
    "release" {
        $preset = "release"
        $exe = "$root/build/release/Release/cube.exe"
    }
    "profile" {
        $preset = "profile"
        $exe = "$root/build/profile/RelWithDebInfo/cube.exe"
    }
    default {
        Write-Error "config must be debug|release|profile"
        exit 1
    }
}

# Run cmake configure and fail fast on non-zero exit code
& cmake --preset $preset
if ($LASTEXITCODE -ne 0) {
    Write-Error "cmake --preset $preset failed (exit code $LASTEXITCODE). Not running the executable."
    exit $LASTEXITCODE
}

# Run build and fail fast on non-zero exit code
& cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) {
    Write-Error "cmake --build --preset $preset failed (exit code $LASTEXITCODE). Not running the executable."
    exit $LASTEXITCODE
}

# Ensure the executable actually exists before running
if (-not (Test-Path $exe)) {
    Write-Error "Executable not found at: $exe. Build reported success but executable missing. Not running anything."
    exit 1
}

# Finally run the executable
& $exe
