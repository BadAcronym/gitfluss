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

# if [[ $? != 0 ]]
# then
#     echo -e "\033[31m\nERROR: failed to compile gitfluss.\n\033[0m"
#     exit -1;
# fi
# 
# echo -e "\n"
# 
# if [[ $2 = "--compile-only" ]]
# then
#     exit 0;
# fi
# 
# popd > /dev/null
# 
# chmod +x ./bin/$build/gitfluss
# ./bin/$build/gitfluss
