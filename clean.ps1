Write-Host "cleaning up gitfluss builds..." -ForegroundColor Yellow

if(-Not(Test-Path "./obj/"))
{
    rm "./obj/" -Recurse -Force
}

if(-Not(Test-Path "./bin/"))
{
    rm "./bin/" -Recurse -Force
}

if(-Not(Test-Path "./build/"))
{
    rm "./build/" -Recurse -Force
}

Write-Host "cleaned gitfluss!`n" -ForegroundColor Green
