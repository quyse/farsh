#ifndef ___FARSH_GENERAL_HPP___
#define ___FARSH_GENERAL_HPP___

#include "../inanity2/inanity-base.hpp"
#include "../inanity2/inanity-graphics.hpp"
#include "../inanity2/inanity-shaders.hpp"
#include "../inanity2/inanity-input.hpp"
#include "../inanity2/inanity-physics.hpp"
#include "../inanity2/inanity-bullet.hpp"
#include "../inanity2/inanity-crypto.hpp"
#include "../inanity2/inanity-lua.hpp"
#include "../inanity2/scripting.hpp"
#include "../inanity2/scripting_decl.hpp"
#include "../inanity2/scripting_impl.hpp"

using namespace Inanity;
using namespace Inanity::Graphics;
using namespace Inanity::Graphics::Shaders;

#if defined(___INANITY_WINDOWS) && 0
#define FARSH_USE_DIRECTX
#else
#define FARSH_USE_OPENGL
#endif

#ifdef FARSH_USE_DIRECTX
#include "../inanity2/inanity-dx.hpp"
#endif
#ifdef FARSH_USE_OPENGL
#include "../inanity2/inanity-gl.hpp"
#endif

#ifndef _DEBUG
//#define PRODUCTION
#endif

#endif
