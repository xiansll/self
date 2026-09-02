# Rewrites the UCRT API-set import names (api-ms-win-crt-*.dll) to ucrtbase.dll.
# Xenos 2.3.2's manual mapper resolves imports by literal module name and cannot
# find "api-ms-win-crt-*.dll" (only "ucrtbase.dll" is loaded in cs2.exe).
# ucrtbase.dll exports every function those API-set aliases forward to, so the
# rewritten imports are fully valid.

param(
    [Parameter(Mandatory=$true)][string]$DllPath
)

if (-not (Test-Path -LiteralPath $DllPath)) {
    throw "DLL not found: $DllPath"
}

$bytes = [System.IO.File]::ReadAllBytes($DllPath)
$needle = [System.Text.Encoding]::ASCII.GetBytes("api-ms-win-crt-")
$replacement = [System.Text.Encoding]::ASCII.GetBytes("ucrtbase.dll")

$count = 0
$i = 0
while ($i -le ($bytes.Length - $needle.Length)) {
    $match = $true
    for ($j = 0; $j -lt $needle.Length; $j++) {
        if ($bytes[$i + $j] -ne $needle[$j]) { $match = $false; break }
    }
    if ($match) {
        # locate end of this null-terminated ASCII name
        $end = $i
        while ($end -lt $bytes.Length -and $bytes[$end] -ne 0) { $end++ }

        # write "ucrtbase.dll" + NUL, then zero-pad the remainder in place
        for ($k = 0; $k -lt $replacement.Length; $k++) {
            $bytes[$i + $k] = $replacement[$k]
        }
        $bytes[$i + $replacement.Length] = 0
        for ($k = $i + $replacement.Length + 1; $k -le $end; $k++) {
            $bytes[$k] = 0
        }
        $count++
        $i = $end + 1
    } else {
        $i++
    }
}

if ($count -eq 0) {
    Write-Host "No api-ms-win-crt-* imports found (already patched?)."
} else {
    [System.IO.File]::WriteAllBytes($DllPath, $bytes)
    Write-Host "Patched $count api-ms-win-crt-* import(s) -> ucrtbase.dll"
}
