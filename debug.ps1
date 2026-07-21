if(-Not(Test-Path "./bin/debug/gitfluss.exe"))
{
    &./run.ps1 debug
}

&raddbg ./bin/debug/gitfluss.exe
