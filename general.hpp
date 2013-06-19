#ifndef ___FARSH_GENERAL_HPP___
#define ___FARSH_GENERAL_HPP___

#include "../inanity/inanity-base.hpp"
#include "../inanity/inanity-graphics.hpp"
#include "../inanity/inanity-shaders.hpp"
#include "../inanity/inanity-input.hpp"
#include "../inanity/inanity-physics.hpp"
#include "../inanity/inanity-bullet.hpp"
#include "../inanity/inanity-platform.hpp"
#include "../inanity/inanity-crypto.hpp"
#include "../inanity/inanity-lua.hpp"
#include "../inanity/meta/decl.hpp"
#include "../inanity/meta/impl.hpp"

using namespace Inanity;
using namespace Inanity::Graphics;
using namespace Inanity::Graphics::Shaders;

#ifdef FARSH_USE_DIRECTX11
#include "../inanity/inanity-dx11.hpp"
#endif
#ifdef FARSH_USE_OPENGL
#include "../inanity/inanity-gl.hpp"
#endif

#ifndef _DEBUG
//#define PRODUCTION
#endif

#endif
