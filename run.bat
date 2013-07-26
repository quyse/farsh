call ice-cpp exe:debug/F.A.R.S.H
if %errorlevel% neq 0 (
	pause
) else (
	debug\F.A.R.S.H.exe > out.txt
)
