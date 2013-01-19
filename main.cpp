#include "general.hpp"
#include "Game.hpp"
#include <sstream>
#include <iostream>
#include <fstream>

#ifdef PRODUCTION
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, INT)
#else
int main()
#endif
{
	try
	{
		MakePointer(NEW(Game()))->Run();
	}
	catch(Exception* exception)
	{
		std::ostringstream s;
		MakePointer(exception)->PrintStack(s);
#ifdef PRODUCTION
		MessageBox(NULL, Strings::UTF82Unicode(s.str()).c_str(), L"F.A.R.S.H. Error", MB_ICONSTOP);
#else
		std::cout << s.str() << '\n';
#endif
	}

	return 0;
}
