$env:SONAR_SCANNER_VERSION = "5.0.0.2966"
$env:SONAR_DIRECTORY = [System.IO.Path]::Combine($(get-location).Path,".sonar")
$env:SONAR_SCANNER_HOME = "$env:SONAR_DIRECTORY/sonar-scanner-$env:SONAR_SCANNER_VERSION-windows"
rm $env:SONAR_SCANNER_HOME -Force -Recurse -ErrorAction SilentlyContinue
New-Item -path $env:SONAR_SCANNER_HOME -type directory
(New-Object System.Net.WebClient).DownloadFile("https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-$env:SONAR_SCANNER_VERSION-windows.zip", "$env:SONAR_DIRECTORY/sonar-scanner.zip")
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory("$env:SONAR_DIRECTORY/sonar-scanner.zip", "$env:SONAR_DIRECTORY")
rm ./.sonar/sonar-scanner.zip -Force -ErrorAction SilentlyContinue
$env:SONAR_SCANNER_OPTS="-server"

rm "$env:SONAR_DIRECTORY/build-wrapper-win-x86" -Force -Recurse -ErrorAction SilentlyContinue
(New-Object System.Net.WebClient).DownloadFile("https://sonarcloud.io/static/cpp/build-wrapper-win-x86.zip", "$env:SONAR_DIRECTORY/build-wrapper-win-x86.zip")
[System.IO.Compression.ZipFile]::ExtractToDirectory("$env:SONAR_DIRECTORY/build-wrapper-win-x86.zip", "$env:SONAR_DIRECTORY")

if ($args[0] -eq "--build") {
    & $env:SONAR_DIRECTORY/build-wrapper-win-x86/build-wrapper-win-x86-64.exe --out-dir bw-output cmake --build $env:BUILD_DIR
} elseif($args[0] -eq "--analyze") {
    $defines = @(
        "sonar.organization=owncloud-1",
        "sonar.projectKey=owncloud_client",
        "sonar.sources=.",
        "sonar.cfamily.build-wrapper-output=bw-output",
        "sonar.host.url=https://sonarcloud.io",
        "sonar.exclusions=docs/**,man/**,translations/**,admin/**,test/**,src/3rdparty/qtokenizer/test/**",
        "sonar.cfamily.threads=$env:NUMBER_OF_PROCESSORS",
        "sonar.cfamily.cache.enabled=true",
        "sonar.cfamily.cache.path=${HOME}/cache/sonarcloud")
    for($i = 0; $i -lt $defines.Length; $i++) {
        $defines[$i] = ("-D{0}" -f $defines[$i])
    }
    & $env:SONAR_SCANNER_HOME/bin/sonar-scanner.bat @defines
}
