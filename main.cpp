#include "general.hpp"
#include "Painter.hpp"
#include "ShaderCache.hpp"
#include "Skeleton.hpp"
#include "BoneAnimation.hpp"
#include "../inanity2/inanity-sqlitefs.hpp"
#include <sstream>
#include <iostream>
#include <fstream>

#define TEST_GRAPHICS_DIRECTX
//#define TEST_GRAPHICS_OPENGL

#include "test.hpp"

#define ZZZ

struct Vertex
{
	float3 position;
	float3 normal;
	float2 texcoord;
};

class Game : public Object
{
private:
	ptr<Window> window;
	ptr<Device> device;
	ptr<Context> context;
	ptr<Presenter> presenter;

	ptr<Painter> painter;

	ptr<FileSystem> fs;

	ptr<Input::Manager> inputManager;

	float alpha;

	ptr<Geometry> geometryCube, geometryKnot, geometryZombi;
	ptr<Painter::Material> texturedMaterial, greenMaterial;
	ptr<Skeleton> skeletonZombi;
	ptr<BoneAnimation> baZombi;
	ptr<BoneAnimationFrame> bafZombi;
	ptr<Geometry> geometryAxe;
	ptr<BoneAnimationFrame> bafAxe;

	PresentMode mode;

	long long lastTick;
	float tickCoef;

	float cameraAlpha, cameraBeta;

	ptr<Physics::World> physicsWorld;
	struct Cube
	{
		ptr<Physics::RigidBody> rigidBody;
		float3 scale;
		Cube(ptr<Physics::RigidBody> rigidBody, const float3& scale = float3(1, 1, 1))
		: rigidBody(rigidBody), scale(scale) {}
	};
	std::vector<Cube> cubes;
	ptr<Physics::Shape> cubePhysicsShape;

	ptr<Physics::Character> physicsCharacter;

	float bloomLimit, toneLuminanceKey, toneMaxLuminance;

public:
	Game() : lastTick(0), cameraAlpha(0), cameraBeta(0),
		bloomLimit(0.9f), toneLuminanceKey(0.8f), toneMaxLuminance(3.1f)
	{
		tickCoef = 1.0f / Time::GetTicksPerSecond();
	}

	void onTick(int)
	{
		long long tick = Time::GetTicks();
		float frameTime = lastTick ? (tick - lastTick) * tickCoef : 0;
		lastTick = tick;

		static int fpsTickCount = 0;
		static float fpsTimeSum = 0;
		static const int fpsTickSpan = 100;
		fpsTimeSum += frameTime;
		if(++fpsTickCount >= fpsTickSpan)
		{
			printf("FPS: %.6f\n", fpsTickSpan / fpsTimeSum);
			fpsTickCount = 0;
			fpsTimeSum = 0;
		}

		static float theTime = 0;
		static bool theTimePaused = false;

		const float maxAngleChange = frameTime * 50;

		ptr<Input::Frame> inputFrame = inputManager->GetCurrentFrame();
		while(inputFrame->NextEvent())
		{
			const Input::Event& inputEvent = inputFrame->GetCurrentEvent();

			//PrintInputEvent(inputEvent);

			switch(inputEvent.device)
			{
			case Input::Event::deviceKeyboard:
				if(inputEvent.keyboard.type == Input::Event::Keyboard::typeKeyDown)
				{
					switch(inputEvent.keyboard.key)
					{
					case 27: // escape
						window->Close();
						return;
					case 32:
						physicsCharacter.FastCast<Physics::BtCharacter>()->GetInternalController()->jump();
						break;
#ifndef PRODUCTION
					case 'Z':
						{
							ptr<Physics::RigidBody> rigidBody = physicsWorld->CreateRigidBody(cubePhysicsShape, 100, physicsCharacter->GetTransform());
							rigidBody->ApplyImpulse(float3(cos(cameraAlpha) * cos(cameraBeta), sin(cameraAlpha) * cos(cameraBeta), sin(cameraBeta)) * 1000);
							cubes.push_back(rigidBody);
						}
						break;
					case 'X':
						theTimePaused = !theTimePaused;
						break;
					case 'R':
						LoadTextures();
						break;

					case '1':
						bloomLimit -= 0.1f;
						printf("bloomLimit: %f\n", bloomLimit);
						break;
					case '2':
						bloomLimit += 0.1f;
						printf("bloomLimit: %f\n", bloomLimit);
						break;
					case '3':
						toneLuminanceKey -= 0.1f;
						printf("toneLuminanceKey: %f\n", toneLuminanceKey);
						break;
					case '4':
						toneLuminanceKey += 0.1f;
						printf("toneLuminanceKey: %f\n", toneLuminanceKey);
						break;
					case '5':
						toneMaxLuminance -= 0.1f;
						printf("toneMaxLuminance: %f\n", toneMaxLuminance);
						break;
					case '6':
						toneMaxLuminance += 0.1f;
						printf("toneMaxLuminance: %f\n", toneMaxLuminance);
						break;
#endif
					}
				}
				break;
			case Input::Event::deviceMouse:
				switch(inputEvent.mouse.type)
				{
				case Input::Event::Mouse::typeButtonDown:
					break;
				case Input::Event::Mouse::typeButtonUp:
					break;
				case Input::Event::Mouse::typeMove:
					cameraAlpha -= std::max(std::min(inputEvent.mouse.offsetX * 0.005f, maxAngleChange), -maxAngleChange);
					cameraBeta -= std::max(std::min(inputEvent.mouse.offsetY * 0.005f, maxAngleChange), -maxAngleChange);
					break;
				}
				break;
			}
		}

		float3 cameraDirection = float3(cos(cameraAlpha) * cos(cameraBeta), sin(cameraAlpha) * cos(cameraBeta), sin(cameraBeta));
		float3 cameraRightDirection = normalize(cross(cameraDirection, float3(0, 0, 1)));
		float3 cameraUpDirection = cross(cameraRightDirection, cameraDirection);

		const Input::State& inputState = inputFrame->GetCurrentState();
		/*
		left up right down Q E
		37 38 39 40
		65 87 68 83 81 69
		*/
		float cameraStep = 2;
		float3 cameraMove(0, 0, 0);
		float3 cameraMoveDirectionFront(cos(cameraAlpha), sin(cameraAlpha), 0);
		float3 cameraMoveDirectionUp(0, 0, 1);
		float3 cameraMoveDirectionRight = cross(cameraMoveDirectionFront, cameraMoveDirectionUp);
		if(inputState.keyboard[37] || inputState.keyboard[65])
			cameraMove -= cameraMoveDirectionRight * cameraStep;
		if(inputState.keyboard[38] || inputState.keyboard[87])
			cameraMove += cameraMoveDirectionFront * cameraStep;
		if(inputState.keyboard[39] || inputState.keyboard[68])
			cameraMove += cameraMoveDirectionRight * cameraStep;
		if(inputState.keyboard[40] || inputState.keyboard[83])
			cameraMove -= cameraMoveDirectionFront * cameraStep;
		if(inputState.keyboard[81])
			cameraMove -= cameraMoveDirectionUp * cameraStep;
		if(inputState.keyboard[69])
			cameraMove += cameraMoveDirectionUp * cameraStep;

		float3 cameraPosition;
		{
			//cameraRigidBody.FastCast<Physics::BtRigidBody>()->GetInternalObject()->proceedToTransform(Physics::toBt(CreateTranslationMatrix(cameraPosition)));
			physicsCharacter->Walk(cameraMove);
			float4x4 t = physicsCharacter->GetTransform();
			cameraPosition = float3(t.t[3][0], t.t[3][1], t.t[3][2] + 0.8f);
		}

		context->Reset();

		alpha += frameTime;

		float4x4 viewMatrix = CreateLookAtMatrix(cameraPosition, cameraPosition + cameraDirection, float3(0, 0, 1));
		float4x4 projMatrix = CreateProjectionPerspectiveFovMatrix(3.1415926535897932f / 4, float(mode.width) / float(mode.height), 0.1f, 100.0f);

		//float3 shadowLightPosition = cameraPosition + cameraMoveDirectionFront * 2.0f + cameraMoveDirectionRight * 10.0f + cameraMoveDirectionUp * 10.0f;
		float3 shadowLightPosition = cameraPosition + cameraMoveDirectionRight * 0.5f;
		//float3 shadowLightPosition(10, 0, 10);
		//float4x4 shadowLightTransform = CreateLookAtMatrix(shadowLightPosition, cameraPosition + cameraMoveDirectionFront * 2.0f, float3(0, 0, 1)) * CreateProjectionPerspectiveFovMatrix(3.1415926535897932f / 4, 1, 1, 100);
		float4x4 shadowLightTransform = CreateLookAtMatrix(shadowLightPosition, cameraPosition + cameraMoveDirectionFront * 1.0f, float3(0, 0, 1)) * CreateProjectionPerspectiveFovMatrix(3.1415926535897932f / 4, 1, 1, 100);
		float3 shadowLightPosition2(20, 20, 10);
		float4x4 shadowLightTransform2 = CreateLookAtMatrix(shadowLightPosition2, float3(10, 10, 0), float3(0, 0, 1)) * CreateProjectionPerspectiveFovMatrix(3.1415926535897932f / 4, 1, 1, 100);

		physicsWorld->Simulate(frameTime);

		// зарегистрировать все объекты
		painter->BeginFrame(frameTime);
		painter->SetCamera(viewMatrix * projMatrix, cameraPosition);
		painter->SetAmbientColor(float3(0, 0, 0));
		//painter->SetAmbientColor(float3(0.2f, 0.2f, 0.2f));
		for(size_t i = 0; i < cubes.size(); ++i)
			painter->AddModel(texturedMaterial, geometryCube, CreateScalingMatrix(cubes[i].scale) * cubes[i].rigidBody->GetTransform());
		//painter->AddModel(greenMaterial, geometryKnot, CreateScalingMatrix(0.3f, 0.3f, 0.3f) * CreateTranslationMatrix(10, 10, 2));
		//painter->AddModel(greenMaterial, geometryZombi, CreateTranslationMatrix(10, 10, 0));

		float intPart;
		if(!theTimePaused)
			theTime += frameTime;
#ifdef ZZZ
		float animationTime = modf(theTime / 11, &intPart) * 11;
		//bafZombi->Setup(float3(10, 10, 0), quaternion(0, 0, 0, 1), modf(theTime * 0.1f, &intPart));
		//bafZombi->Setup(float3(10, 15, 0), quaternion(0, 0, 0, 1), modf(theTime * 0.1f, &intPart) * 5);
		bafZombi->Setup(float3(10, 15, 0), quaternion(0, 0, 0, 1), animationTime);
		bafAxe->Setup(float3(10, 15, 0), quaternion(0, 0, 0, 1), animationTime);
		//bafZombi->Setup(float3(10, 15, 0), quaternion(float3(0, 0, 1), modf(theTime * 0.1f, &intPart)), 0);
#else
		bafZombi->Setup(float3(10, 10, 1), quaternion(0, 0, 0, 1), modf(theTime / 4, &intPart) * 4);
#endif

//		for(size_t i = 0; i < bafZombi->animationWorldPositions.size(); ++i)
//			painter->AddModel(texturedMaterial, geometryCube, (float4x4)bafZombi->orientations[i] * CreateScalingMatrix(0.1f, 0.1f, 0.1f) * CreateTranslationMatrix(bafZombi->animationWorldPositions[i]));

		//bafZombi->Setup(float3(10, 10, 1), quaternion(0, 0, 0, 1), modf(t * 0.1f, &intPart));
		//bafZombi->Setup(float3(10, 10, 1), quaternion(float3(1, 0, 0), modf(t / 3, &intPart) * 3), 0);
		//bafZombi->Setup(float3(10, 10, 1), quaternion(0, 0, 0, 1), 0);
		//bafZombi->orientations[2] = quaternion(float3(1, 0, 0), modf(t / 3, &intPart)) * bafZombi->orientations[2];
		painter->AddSkinnedModel(greenMaterial, geometryZombi, bafZombi);
		painter->AddModel(greenMaterial, geometryAxe, (float4x4)bafAxe->animationWorldOrientations[0] * CreateTranslationMatrix(bafAxe->animationWorldPositions[0]));

		painter->AddShadowLight(shadowLightPosition, float3(1.0f, 1.0f, 1.0f) * 0.4f, shadowLightTransform);
		painter->AddShadowLight(shadowLightPosition2, float3(1.0f, 1.0f, 1.0f) * 0.2f, shadowLightTransform2);

		painter->SetupPostprocess(bloomLimit, toneLuminanceKey, toneMaxLuminance);

		painter->Draw();

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
		this->window = window;
		window->SetTitle("F.A.R.S.H.");

		inputManager = NEW(Input::RawManager(window->GetHWND()));
		window->SetInputManager(inputManager);

#if defined(_DEBUG) && 1
		mode.width = 800;
		mode.height = 600;
		mode.fullscreen = false;
#else
		mode.width = GetSystemMetrics(SM_CXSCREEN);
		mode.height = GetSystemMetrics(SM_CYSCREEN);
		mode.fullscreen = true;
#endif
		mode.pixelFormat = PixelFormats::intR8G8B8A8;
		presenter = device->CreatePresenter(window->CreateOutput(), mode);

		context = device->GetContext();

		const char* shadersCacheFileName =
#ifdef _DEBUG
			"shaders_debug"
#else
			"shaders"
#endif
			;
		ptr<ShaderCache> shaderCache = NEW(ShaderCache(NEW(SQLiteFileSystem(shadersCacheFileName)), device, NEW(DxShaderCompiler())));
		painter = NEW(Painter(device, context, presenter, mode.width, mode.height, shaderCache));

		// разметка
		std::vector<Layout::Element> layoutElements;
		layoutElements.push_back(Layout::Element(DataTypes::Float3, 0, 0));
		layoutElements.push_back(Layout::Element(DataTypes::Float3, 12, 1));
		layoutElements.push_back(Layout::Element(DataTypes::Float2, 24, 2));
		ptr<Layout> layout = NEW(Layout(layoutElements, sizeof(Vertex)));

		// разметка для skinning
		layoutElements.push_back(Layout::Element(DataTypes::UInt4, 32, 3));
		layoutElements.push_back(Layout::Element(DataTypes::Float4, 48, 4));
		ptr<Layout> skinLayout = NEW(Layout(layoutElements, 64));

		alpha = 0;

		fs = FolderFileSystem::GetNativeFileSystem();

		geometryCube = NEW(Geometry(
			device->CreateVertexBuffer(fs->LoadFile("box.geo.vertices"), layout),
			device->CreateIndexBuffer(fs->LoadFile("box.geo.indices"), sizeof(short))));
		geometryKnot = NEW(Geometry(
			device->CreateVertexBuffer(fs->LoadFile("knot.geo.vertices"), layout),
			device->CreateIndexBuffer(fs->LoadFile("knot.geo.indices"), sizeof(short))));

#ifdef ZZZ
		geometryZombi = NEW(Geometry(
			device->CreateVertexBuffer(fs->LoadFile("zombi.geo.vertices"), skinLayout),
			device->CreateIndexBuffer(fs->LoadFile("zombi.geo.indices"), sizeof(short))));
		skeletonZombi = Skeleton::Deserialize(fs->LoadFileAsStream("zombi.skeleton"));
		baZombi = BoneAnimation::Deserialize(fs->LoadFileAsStream("zombi.ba"), skeletonZombi);
		bafZombi = NEW(BoneAnimationFrame(baZombi));
#else
		geometryZombi = NEW(Geometry(
			device->CreateVertexBuffer(fs->LoadFile("og.geo.vertices"), skinLayout),
			device->CreateIndexBuffer(fs->LoadFile("og.geo.indices"), sizeof(short))));
		skeletonZombi = Skeleton::Deserialize(fs->LoadFileAsStream("og.skeleton"));
		baZombi = BoneAnimation::Deserialize(fs->LoadFileAsStream("og.ba"), skeletonZombi);
		bafZombi = NEW(BoneAnimationFrame(baZombi));
#endif

		{
			geometryAxe = NEW(Geometry(
				device->CreateVertexBuffer(fs->LoadFile("axe.geo.vertices"), layout),
				device->CreateIndexBuffer(fs->LoadFile("axe.geo.indices"), sizeof(short))));
			std::vector<Skeleton::Bone> bones(1);
			bones[0].originalWorldPosition = float3(0, 0, 0);
			bones[0].originalRelativePosition = bones[0].originalWorldPosition;
			bones[0].parent = 0;
			bafAxe = NEW(BoneAnimationFrame(BoneAnimation::Deserialize(fs->LoadFileAsStream("axe.ba"), NEW(Skeleton(bones)))));
		}

		texturedMaterial = NEW(Painter::Material());
		greenMaterial = NEW(Painter::Material());
		greenMaterial->diffuse = float4(0, 1, 0, 1);

		LoadTextures();

		physicsWorld = NEW(Physics::BtWorld());

		// пол и кубики лабиринта
		{
			std::fstream f("labyrint.txt", std::ios::in);
			int n, m;
			f >> n >> m;

			float cellHalfSize = 1.0f;

			//const float modelScale = 0.002f;
			const float modelScale = 1.0f;

			// пол
			cubes.push_back(Cube(
				physicsWorld->CreateRigidBody(
					physicsWorld->CreateBoxShape(float3(float(m) * cellHalfSize, float(n) * cellHalfSize, cellHalfSize)),
					0, CreateTranslationMatrix(float(m) * cellHalfSize, float(n) * cellHalfSize, -cellHalfSize)),
					float3(float(m), float(n), 1) * cellHalfSize * modelScale
				));

			// стенки
			ptr<Physics::Shape> wallShape = physicsWorld->CreateBoxShape(float3(cellHalfSize, cellHalfSize, cellHalfSize));
			std::string line;
			for(int i = 0; i < n; ++i)
			{
				f >> line;
				for(int j = 0; j < m; ++j)
					switch(line[j])
					{
					case '#':
						for(int k = 0; k < 2; ++k)
							cubes.push_back(Cube(
								physicsWorld->CreateRigidBody(wallShape, 0,
									CreateTranslationMatrix(float(j) * cellHalfSize * 2 + cellHalfSize, float(i) * cellHalfSize * 2 + cellHalfSize, float(k) * cellHalfSize * 2 + cellHalfSize)),
								float3(cellHalfSize, cellHalfSize, cellHalfSize) * modelScale));
						break;
					case '$':
						physicsCharacter = physicsWorld->CreateCharacter(
							physicsWorld->CreateCapsuleShape(0.4f, 1),
							CreateTranslationMatrix(float(j) * cellHalfSize * 2 + cellHalfSize, float(i) * cellHalfSize * 2 + cellHalfSize, 50));
						break;
					}
			}
		}

		// падающие кубики
		cubePhysicsShape = physicsWorld->CreateBoxShape(float3(1, 1, 1));
		if(0)
		{
			for(int i = 0; i < 5; ++i)
				for(int j = 0; j < 5; ++j)
					for(int k = 0; k < 5; ++k)
					{
						ptr<Physics::RigidBody> rigidBody = physicsWorld->CreateRigidBody(cubePhysicsShape, 10.0f, CreateTranslationMatrix(i * 4.0f - k * 1.0f - 2.0f, j * 4.0f - k * 0.5f - 2.0f, k * 4.0f + 10.0f));
						cubes.push_back(rigidBody);
					}
		}

		window->Run(Win32Window::ActiveHandler::CreateDelegate(MakePointer(this), &Game::onTick));
	}

	void LoadTextures()
	{
		ptr<Texture> a = device->CreateStaticTexture(fs->LoadFile("diffuse.jpg"));
		ptr<Texture> b = device->CreateStaticTexture(fs->LoadFile("specular.png"));
		texturedMaterial->diffuseTexture = a;
		texturedMaterial->specularTexture = b;
		greenMaterial->specularTexture = b;
	}
};

template <int n>
std::ostream& operator<<(std::ostream& s, vector<n> v)
{
	for(int i = 0; i < n; ++i)
	{
		if(i) s << ' ';
		s << v.t[i];
	}
	return s;
}

#ifdef _DEBUG
int main()
#else
int main()
//int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, INT)
#endif
{
	//freopen("output.txt", "w", stdout);

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
