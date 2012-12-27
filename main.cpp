#include "../inanity2/inanity-base.hpp"
#include "../inanity2/inanity-graphics.hpp"
#include "../inanity2/inanity-dx.hpp"
#include "../inanity2/inanity-shaders.hpp"
#include <sstream>
#include <iostream>

#define TEST_GRAPHICS_DIRECTX
//#define TEST_GRAPHICS_OPENGL

using namespace Inanity;
using namespace Inanity::Graphics;
using namespace Inanity::Graphics::Shaders;

struct Vertex
{
	float3 position;
	float3 normal;
	float2 texcoord;
};

class TestShader
{
private:
	Value<float4> aPosition;
	Value<float3> aNormal;
	Value<float2> aTexcoord0;

	ptr<UniformGroup> ugCamera;
	Uniform<float4x4> uViewProj;
	Uniform<float3> uCameraPosition;

	ptr<UniformGroup> ugLight;
	Uniform<float3> uLightPosition;
	Uniform<float3> uLightDirection;

	ptr<UniformGroup> ugMaterial;
	Sampler<float3, float2> uDiffuseSampler;

	ptr<UniformGroup> ugModel;
	Uniform<float4x4> uWorldViewProj;
	Uniform<float4x4> uWorld;

public:
	TestShader() :
		aPosition(NEW(AttributeNode(0, DataTypes::Float4))),
		aNormal(NEW(AttributeNode(12, DataTypes::Float3))),
		aTexcoord0(NEW(AttributeNode(24, DataTypes::Float2))),

		ugCamera(NEW(UniformGroup())),
		uViewProj(ugCamera->AddUniform<float4x4>()),
		uCameraPosition(ugCamera->AddUniform<float3>()),

		ugLight(NEW(UniformGroup())),
		uLightPosition(ugLight->AddUniform<float3>()),
		uLightDirection(ugLight->AddUniform<float3>()),

		ugMaterial(NEW(UniformGroup())),
		uDiffuseSampler(0),

		ugModel(NEW(UniformGroup())),
		uWorldViewProj(ugModel->AddUniform<float4x4>()),
		uWorld(ugModel->AddUniform<float4x4>())
	{
	}
};

class Game : public Object
{
private:
	ptr<Device> device;
	ptr<Context> context;
	ptr<Presenter> presenter;

	ContextState drawingState;

public:
	void onTick(int)
	{
		float color[4] = { 1, 0, 0, 0 };
		context->ClearRenderBuffer(presenter->GetBackBuffer(), color);
		presenter->Present();
	}

	void process()
	{
#ifdef TEST_GRAPHICS_DIRECTX
		ptr<Graphics::System> system = NEW(DxSystem());
#endif
#ifdef TEST_GRAPHICS_OPENGL
		ptr<Graphics::System> system = NEW(GlSystem());
#endif

		device = system->CreatePrimaryDevice();
		ptr<Win32Window> window = system->CreateDefaultWindow().FastCast<Win32Window>();

		PresentMode mode;
		mode.width = 640;
		mode.height = 480;
		mode.fullscreen = false;
		mode.pixelFormat = PixelFormats::intR8G8B8A8;
		presenter = device->CreatePresenter(window->CreateOutput(), mode);

		context = device->GetContext();

		// разметка
		std::vector<Layout::Element> layoutElements;
		layoutElements.push_back(Layout::Element(DataTypes::Float3, 0, Semantics::CustomPosition));
		layoutElements.push_back(Layout::Element(DataTypes::Float3, 12, Semantics::CustomNormal));
		layoutElements.push_back(Layout::Element(DataTypes::Float2, 24, Semantics::CustomTexcoord0));
		ptr<Layout> layout = NEW(Layout(layoutElements, sizeof(Vertex)));

		// шейдер :)
		TestShader testShader;

		drawingState.renderBuffers[0] = presenter->GetBackBuffer();

		window->Run(Win32Window::ActiveHandler::CreateDelegate(MakePointer(this), &Game::onTick));
	}
};

#ifdef _DEBUG
int main()
#else
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, INT)
#endif
{
	try
	{
		MakePointer(NEW(Game()))->process();
	}
	catch(Exception* exception)
	{
		std::ostringstream s;
		MakePointer(exception)->PrintStack(s);
		std::cout << s.str() << '\n';
	}

	return 0;
}
