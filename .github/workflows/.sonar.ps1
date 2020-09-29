$env:SONAR_SCANNER_VERSION = "4.4.0.2170"
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

& $env:SONAR_DIRECTORY/build-wrapper-win-x86/build-wrapper-win-x86-64.exe --out-dir bw-output cmake --build $env:BUILD_DIR 
& $env:SONAR_SCANNER_HOME/bin/sonar-scanner.bat -D"sonar.organization=owncloud-1" -D"sonar.projectKey=owncloud_client" -D"sonar.sources=." -D"sonar.test.exclusions=test/**" -D"sonar.tests=test/" -D"sonar.cfamily.build-wrapper-output=bw-output" -D"sonar.host.url=https://sonarcloud.io" -D"sonar.exclusions=docs/**,man/**,translations/**,admin/**"
