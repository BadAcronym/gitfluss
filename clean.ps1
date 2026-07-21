Write-Host "cleaning up gitfluss builds..." -ForegroundColor Yellow

if(Test-Path "./obj/")
{
    rm "./obj/" -Recurse -Force
}

if(Test-Path "./bin/")
{
    rm "./bin/" -Recurse -Force
}

if(Test-Path "./build/")
{
    rm "./build/" -Recurse -Force
}

Write-Host "cleaned gitfluss!`n" -ForegroundColor Green
