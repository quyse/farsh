call ice-cpp exe:debug/F.A.R.S.H-dx11
if %errorlevel% neq 0 (
	pause
) else (
	debug\F.A.R.S.H-dx11.exe > out.txt
)
