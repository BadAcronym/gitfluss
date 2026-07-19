param($build)

if(-Not(Test-Path "./obj/"))
{
    mkdir "./obj/"    
}

if(-Not(Test-Path "./build/"))
{
    mkdir "./build/"    
}

if(-Not(Test-Path "./bin/"))
{
    mkdir "./bin/"    
}

if($build -eq $null)
{
    $build = "release"
}

if($build -eq "asan" -or $build -eq "debug" -or $build -eq "release")
{
    Write-Host ""
    Write-Host "compiling gitfluss..." -ForegroundColor Cyan
    Write-Host ""

    premake5 vs2026
    &MSBuild ./build/gitfluss.sln -p:Configuration=$build
}
else
{
     Write-Host "ERROR: invalid make config: '$build'." -ForegroundColor Red
     exit -2;
}

if($LASTEXITCODE -ne 0)
{
     Write-Host "`nERROR: failed to compile gitfluss.`n" -ForegroundColor Red
     exit -1;
}

Write-Host "`n"

 
# if [[ $2 = "--compile-only" ]]
# then
#     exit 0;
# fi
# 
# popd > /dev/null
# 
# chmod +x ./bin/$build/gitfluss
# ./bin/$build/gitfluss
