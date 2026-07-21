param
(
    [Parameter(Position = 0)][string]$build,
    [Parameter(Position = 1)][string]$compile_only
)

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

if($build -eq $null -or $build -eq "")
{
    $build = "release"
}

if($build -eq "asan" -or $build -eq "debug" -or $build -eq "release")
{
    Write-Host "`ncompiling gitfluss...`n" -Fore Cyan

    premake5 gmake
    pushd "./build/"
    make config=$build`_windows
    popd

    $dllPath = "./vendor/libgit2/build/Debug/git2.dll"

    if(Test-Path $dllPath)
    {
        cp $dllPath "./bin/$build/"
    }
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

if($compile_only -eq "--compile-only")
{
    exit 0;
}

&./bin/$build/gitfluss
