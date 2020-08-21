<#
    .SYNOPSIS
      This script replaces the color in all svg files in this folder
#>
$COLOR=$Args[0]
Write-Host "Color:" $COLOR
if (-not $COLOR) {
    Write-Host "Please specify color"
    Write-Host "update_corlor.ps1 `"#B5B6BB`""
    exit(1)
}
Get-ChildItem "*.svg" | % {
    $tmp = Get-Content $_
    $tmp = $tmp -replace "fill=`"\S+`"", "fill=`"$COLOR`""
    Set-Content -Path $_ -Value $tmp

}
