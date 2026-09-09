param
(
    [Parameter(Position = 0)][string]$build,
    [Parameter(Position = 1)][string]$compile_only
)

if(-Not(Test-Path "./bin/" -PathType Container))
{
    mkdir "./bin/"
}

if($build -eq $null -or $build -eq "")
{
    $build = "release"
}

$args_always=@("-DBUILD_WINDOWS",
"src/gitfluss_main.c",
"src/gitfluss_parsing.c",
"src/win32_gitfluss_platform.c",
"vendor/puddle/src/string_view.c",
"vendor/puddle/src/win32_pd_path.c",
"-Iinclude",
"-Ivendor/puddle/include",
"-std=c99",
"-Wextra", "-Wall", "-Wpedantic", "-Wconversion", "-Wshadow", "-Wsign-compare",
"-Wtype-limits", "-Wunused",
"-Wno-unsafe-buffer-usage", "-Wno-declaration-after-statement", "-Wno-vla",
"-Wno-implicit-void-ptr-cast")

$args_release=@("-O2")

$args_debug=@("-DDEBUG", "-gcodeview", "-O0")
$args_debug_cl=@("/DDEBUG", "/Zi", "/Od")

$args_asan=$args_debug_cl+@("-oa.exe", "/clang:-std=c99", "/DASAN",
"/fsanitize=address", "/MD",
"/link", "/SUBSYSTEM:CONSOLE")

function compile
{
    param( [string[]]$1 )

    Write-Host "identifying a compiler..."

    if($build -eq "asan")
    {
        if(-Not(Get-Command clang -ErrorAction SilentlyContinue))
        {
            Write-Host "ERROR: clang-cl needed for address sanitization." -Fore Red
        }
        $script:compiler="clang-cl"
    }
    elseif(Get-Command clang -ErrorAction SilentlyContinue)
    {
        Write-Host "found clang."
        $script:compiler="clang"
    }
    elseif(Get-Command gcc -ErrorAction SilentlyContinue)
    {
        Write-Host "found gcc."
        $script:compiler="gcc"
    }
    else
    {
        Write-Host "ERROR: no suitable compiler found." -Fore Red
    }

    Write-Host ""
    Write-Host "compiling gitfluss..." -Fore Cyan
    Write-Host ""

    if(-Not (Test-Path "./bin/$build/" -PathType Container))
    {
        mkdir "./bin/$build/"
    }

    Write-Host "compiling $build build with the following command:"
    Write-Host "$script:compiler $1"
    &$script:compiler @1
    if($LASTEXITCODE -ne 0)
    {
        Write-Host "`nERROR: $script:compiler failed to compile datasurf.`n" -Fore Red
        exit -1
    }
    Move-Item ./a.exe ./bin/$build/gitfluss.exe -Force
    if($build -eq "release")
    {
        return;
    }
    Move-Item ./a.pdb ./bin/$build/gitfluss.pdb -Force
}

if($build -eq "release")
{
    compile ($args_always + $args_release)
}
elseif($build -eq "debug")
{
    compile ($args_always + $args_debug)
}
elseif($build -eq "asan")
{
    compile ($args_always + $args_asan)
}
else
{
    Write-Host "`nERROR: invalid make config: $build." -Fore Red
    exit 3;
}

Write-Host "`n"

if($compile_only -eq "--compile-only")
{
    exit 0
}

&./bin/$build/gitfluss.exe
