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
    if(-Not(Test-Path "./bin/$build/"))
    {
        mkdir "./bin/$build/"
    }

    if(Test-Path "./vendor/libgit2/build/Debug/git2.dll")
    {
        cp "./vendor/libgit2/build/Debug/git2.dll" "./bin/$build/"
    }

    if(-Not(Test-Path "./bin/$build/git2.dll"))
    {

        Write-Host "WARNING: git2.dll could not be located." -Fore Yellow
        Write-Host "Compiling from source..." -Fore Yellow

        &cmake --version
        if($LASTEXITCODE -ne 0)
        {
            Write-Host "ERROR: CMake not found. Please provide a binary " -NoNewline
            Write-Host " in your path in order to build git2.dll from source." -Fore Red
            exit 1;
        }

        pushd "./vendor/libgit2/"
        if(-Not (Test-Path "build"))
        {
            mkdir "build"
        }
        cd build

        cmake .. -DBUILD_TESTS=OFF
        if($LASTEXITCODE -ne 0)
        {
            Write-Host "ERROR: CMake build failed."
            popd
            exit 3;
        }
        cmake --build .
        if($LASTEXITCODE -ne 0 -or -Not(Test-Path "./vendor/libgit2/build/git2.dll"))
        {
            Write-Host "ERROR: failed to compile libgit2."
            popd
            exit 4;
        }

        popd

        cp "./vendor/libgit2/build/Debug/git2.dll" "./bin/$build"
    }

    Write-Host "`ncompiling gitfluss...`n" -Fore Cyan

    premake5 gmake
    pushd "./build/"
    make config=$build`_windows
    popd
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
