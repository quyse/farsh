#include "../inanity2/inanity-base.hpp"
#include "../inanity2/inanity-graphics.hpp"
#include "../inanity2/inanity-dx.hpp"
#include "../inanity2/inanity-shaders.hpp"
#include <sstream>
#include <iostream>
#include <fstream>

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

struct TestShader
{
	Attribute<float4> aPosition;
	Attribute<float3> aNormal;
	Attribute<float2> aTexcoord;

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

	TestShader() :
		aPosition(0),
		aNormal(1),
		aTexcoord(2),

		ugCamera(NEW(UniformGroup(0))),
		uViewProj(ugCamera->AddUniform<float4x4>()),
		uCameraPosition(ugCamera->AddUniform<float3>()),

		ugLight(NEW(UniformGroup(1))),
		uLightPosition(ugLight->AddUniform<float3>()),
		uLightDirection(ugLight->AddUniform<float3>()),

		ugMaterial(NEW(UniformGroup(2))),
		uDiffuseSampler(0),

		ugModel(NEW(UniformGroup(3))),
		uWorldViewProj(ugModel->AddUniform<float4x4>()),
		uWorld(ugModel->AddUniform<float4x4>())
	{
		ugCamera->Finalize();
		ugLight->Finalize();
		ugMaterial->Finalize();
		ugModel->Finalize();
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

		context->Reset();
		context->GetTargetState() = drawingState;
		context->Draw();

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
		layoutElements.push_back(Layout::Element(DataTypes::Float3, 0, 0));
		layoutElements.push_back(Layout::Element(DataTypes::Float3, 12, 1));
		layoutElements.push_back(Layout::Element(DataTypes::Float2, 24, 2));
		ptr<Layout> layout = NEW(Layout(layoutElements, sizeof(Vertex)));

		// шейдер :)
		TestShader t;

		Interpolant<float4> tPosition(Semantics::VertexPosition);
		Interpolant<float3> tNormal(Semantics::CustomNormal);
		Interpolant<float2> tTexcoord(Semantics::CustomTexcoord0);

		Temp<float4> tmpPosition;
		Temp<float3> tmpNormal;

		Fragment<float4> tTarget(Semantics::TargetColor0);

		Expression vertexShader = (
			tPosition = t.aPosition
			);

		Expression pixelShader = (
				tTarget = newfloat4(0, 1, 0, 1)
			);

		ptr<HlslGenerator> shaderGenerator = NEW(HlslGenerator());
		ptr<ShaderSource> vertexShaderSource = shaderGenerator->Generate(vertexShader, ShaderTypes::vertex);
		ptr<ShaderSource> pixelShaderSource = shaderGenerator->Generate(pixelShader, ShaderTypes::pixel);

		ptr<DxShaderCompiler> shaderCompiler = NEW(DxShaderCompiler());
		ptr<File> vertexShaderBinary = shaderCompiler->Compile(vertexShaderSource);
		ptr<File> pixelShaderBinary = shaderCompiler->Compile(pixelShaderSource);

		ptr<FileSystem> fs = FolderFileSystem::GetNativeFileSystem();
		fs->SaveFile(vertexShaderSource->GetCode(), "vs.fx");
		fs->SaveFile(pixelShaderSource->GetCode(), "ps.fx");
		fs->SaveFile(vertexShaderBinary, "vs.fxo");
		fs->SaveFile(pixelShaderBinary, "ps.fxo");

		float4x4 viewMatrix = CreateLookAtMatrix(float3(100, 100, 100), float3(0, 0, 0), float3(0, 0, 1));
		t.uWorldViewProj.SetValue(viewMatrix);

		drawingState.viewportWidth = mode.width;
		drawingState.viewportHeight = mode.height;
		drawingState.renderBuffers[0] = presenter->GetBackBuffer();
		drawingState.vertexShader = device->CreateVertexShader(vertexShaderBinary);
		drawingState.pixelShader = device->CreatePixelShader(pixelShaderBinary);

#if 0
		ptr<File> vertexBufferFile = NEW(MemoryFile(sizeof(Vertex) * 3));
		Vertex* vertexBufferData = (Vertex*)vertexBufferFile->GetData();
		vertexBufferData[0].position = float3(-0.5f, -0.5f, 0);
		vertexBufferData[1].position = float3(0, 0.5f, 0);
		vertexBufferData[2].position = float3(0.5, 0, 0);
		drawingState.vertexBuffer = device->CreateVertexBuffer(vertexBufferFile, layout);
		ptr<File> indexBufferFile = NEW(MemoryFile(sizeof(short) * 3));
		short* indexBufferData = (short*)indexBufferFile->GetData();
		indexBufferData[0] = 0;
		indexBufferData[1] = 1;
		indexBufferData[2] = 2;
		drawingState.indexBuffer = device->CreateIndexBuffer(indexBufferFile, sizeof(short));
#else
#endif

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
