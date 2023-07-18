cd "${env:GITHUB_WORKSPACE}"
git ls-files *.cpp | ForEach-Object -Parallel {clang-tidy -p $env:BUILD_DIR/compile_commands.json $_ } -ThrottleLimit 5 | Tee-Object -Path $env:TMPDIR/clang-tidy.log
