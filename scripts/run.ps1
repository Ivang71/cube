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

cmake --preset $preset
cmake --build --preset $preset
& $exe