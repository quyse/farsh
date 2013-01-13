set NOPROGRESS=1
call ice-cpp exe:release/F.A.R.S.H
if %errorlevel% neq 0 (
	pause
) else (
	release\F.A.R.S.H.exe
)
