cd "${env:GITHUB_WORKSPACE}"

# allow errors as some files can't be compiled on all platforms
$oldErrorActio = $ErrorActionPreference
$ErrorActionPreference="Continue"
git ls-files *.cpp | ForEach-Object -Parallel {clang-tidy -p $env:BUILD_DIR/compile_commands.json $_ } -ThrottleLimit 5 | Tee-Object -Path $env:TMPDIR/clang-tidy.log
$ErrorActionPreference = $oldErrorActio
