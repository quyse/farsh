#include "general.hpp"
#include "Game.hpp"
#include "GeometryFormats.hpp"
#include "Painter.hpp"
#include "Material.hpp"
#include "Geometry.hpp"
#include "Skeleton.hpp"
#include "BoneAnimation.hpp"
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
		std::fstream("error.txt", std::ios::out) << s.str() << '\n';
#else
		std::cout << s.str() << '\n';
#endif
	}

	return 0;
}
