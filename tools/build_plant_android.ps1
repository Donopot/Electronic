param([string]$EnvironmentFile = '.tools/android-bootstrap/environment.json')
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
if (Test-Path -LiteralPath (Join-Path $workspace $EnvironmentFile)) {
    $configuration = Get-Content -Raw -LiteralPath (Join-Path $workspace $EnvironmentFile) | ConvertFrom-Json
    $env:JAVA_HOME = $configuration.JAVA_HOME
    $env:ANDROID_HOME = $configuration.ANDROID_HOME
    $env:GRADLE_USER_HOME = $configuration.GRADLE_USER_HOME
    $gradleCommand = $configuration.GRADLE
} else {
    if (-not $env:JAVA_HOME -or -not $env:ANDROID_HOME) { throw 'Set JAVA_HOME (JDK 17) and ANDROID_HOME (SDK 35/build-tools 35.0.0), or supply the prepared environment file.' }
    $gradleCommand = Join-Path $workspace 'android/PlantStation/gradlew.bat'
}
if (-not (Test-Path -LiteralPath (Join-Path $workspace '.tools/private/plantstation-signing.properties'))) {
    throw 'Prepare the private local signing key and .tools/private/plantstation-signing.properties as described in android/PlantStation/README.md.'
}
& $gradleCommand -p (Join-Path $workspace 'android/PlantStation') --no-daemon :app:testDebugUnitTest :app:lintDebug :app:assembleRelease
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Output (Join-Path $workspace 'android/PlantStation/app/build/outputs/apk/release/app-release.apk')
