#include "Game.hpp"
#include "Painter.hpp"
#include "Material.hpp"
#include "ShaderCache.hpp"
#include "Skeleton.hpp"
#include "BoneAnimation.hpp"
#include "../inanity2/inanity-sqlitefs.hpp"
#include <iostream>

SCRIPTABLE_MAP_BEGIN(Game, Farsh.Game);
	SCRIPTABLE_METHOD(Game, Get);
	SCRIPTABLE_METHOD(Game, LoadTexture);
	SCRIPTABLE_METHOD(Game, LoadGeometry);
	SCRIPTABLE_METHOD(Game, LoadSkinnedGeometry);
	SCRIPTABLE_METHOD(Game, LoadSkeleton);
	SCRIPTABLE_METHOD(Game, LoadBoneAnimation);
	SCRIPTABLE_METHOD(Game, CreatePhysicsBoxShape);
	SCRIPTABLE_METHOD(Game, CreatePhysicsRigidBody);
	SCRIPTABLE_METHOD(Game, AddStaticModel);
	SCRIPTABLE_METHOD(Game, AddRigidModel);
	SCRIPTABLE_METHOD(Game, AddStaticLight);
SCRIPTABLE_MAP_END();

Game* Game::singleGame = 0;

Game::Game() :
	lastTick(0), cameraAlpha(0), cameraBeta(0),
	bloomLimit(10.0f), toneLuminanceKey(0.12f), toneMaxLuminance(3.1f)
{
	singleGame = this;

	tickCoef = 1.0f / Time::GetTicksPerSecond();

	// разметка
	std::vector<Layout::Element> layoutElements;
	layoutElements.push_back(Layout::Element(DataTypes::Float3, 0, 0));
	layoutElements.push_back(Layout::Element(DataTypes::Float3, 12, 1));
	layoutElements.push_back(Layout::Element(DataTypes::Float2, 24, 2));
	layout = NEW(Layout(layoutElements, 32));

	// разметка для skinning
	layoutElements.push_back(Layout::Element(DataTypes::UInt4, 32, 3));
	layoutElements.push_back(Layout::Element(DataTypes::Float4, 48, 4));
	skinnedLayout = NEW(Layout(layoutElements, 64));
}

void Game::Run()
{
	ptr<Graphics::System> system = NEW(DxSystem());

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

	fileSystem =
#ifdef PRODUCTION
		NEW(BlobFileSystem(FolderFileSystem::GetNativeFileSystem()->LoadFile("data")))
#else
		FolderFileSystem::GetNativeFileSystem()
#endif
	;

	physicsWorld = NEW(Physics::BtWorld());

	// запустить стартовый скрипт
	scriptState = NEW(Lua::State());
	scriptState->RegisterClass<Game>();
	scriptState->RegisterClass<Material>();

	ptr<Script> mainScript = scriptState->LoadScript(fileSystem->LoadFile("main.lua"));
	mainScript->Run<void>();

	window->Run(Win32Window::ActiveHandler::CreateDelegate(MakePointer(this), &Game::Tick));
}

void Game::Tick(int)
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
					//physicsCharacter.FastCast<Physics::BtCharacter>()->GetInternalController()->jump();
					break;
#ifndef PRODUCTION
				case 'M':
					try
					{
						scriptState->LoadScript(fileSystem->LoadFile("console.lua"))->Run<void>();
						std::cout << "console.lua successfully executed.\n";
					}
					catch(Exception* exception)
					{
						std::ostringstream s;
						MakePointer(exception)->PrintStack(s);
						std::cout << s.str() << '\n';
					}
					break;
				case 'Z':
					{
						//ptr<Physics::RigidBody> rigidBody = physicsWorld->CreateRigidBody(cubePhysicsShape, 100, physicsCharacter->GetTransform());
						//rigidBody->ApplyImpulse(float3(cos(cameraAlpha) * cos(cameraBeta), sin(cameraAlpha) * cos(cameraBeta), sin(cameraBeta)) * 1000);
						//cubes.push_back(rigidBody);
					}
					break;
				case 'X':
					theTimePaused = !theTimePaused;
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
					toneLuminanceKey -= 0.01f;
					printf("toneLuminanceKey: %f\n", toneLuminanceKey);
					break;
				case '4':
					toneLuminanceKey += 0.01f;
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

				case '7':
					zombieMaterial->specular.x -= 0.01f;
					zombieMaterial->specular.y = zombieMaterial->specular.x;
					zombieMaterial->specular.z = zombieMaterial->specular.x;
					printf("specular: %f\n", zombieMaterial->specular.x);
					break;
				case '8':
					zombieMaterial->specular.x += 0.01f;
					zombieMaterial->specular.y = zombieMaterial->specular.x;
					zombieMaterial->specular.z = zombieMaterial->specular.x;
					printf("specular: %f\n", zombieMaterial->specular.x);
					break;
				case '9':
					zombieMaterial->specular.w -= 0.01f;
					printf("glossiness: %f\n", zombieMaterial->specular.w);
					break;
				case '0':
					zombieMaterial->specular.w += 0.01f;
					printf("glossiness: %f\n", zombieMaterial->specular.w);
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

	static float3 cameraPosition = float3(-10, 0, 0);
	cameraPosition += cameraMove * frameTime;

	context->Reset();

	alpha += frameTime;

	float4x4 viewMatrix = CreateLookAtMatrix(cameraPosition, cameraPosition + cameraDirection, float3(0, 0, 1));
	float4x4 projMatrix = CreateProjectionPerspectiveFovMatrix(3.1415926535897932f / 4, float(mode.width) / float(mode.height), 0.1f, 100.0f);

	physicsWorld->Simulate(frameTime);

	// зарегистрировать все объекты
	painter->BeginFrame(frameTime);
	painter->SetCamera(viewMatrix * projMatrix, cameraPosition);
	painter->SetAmbientColor(float3(0, 0, 0));

	for(size_t i = 0; i < staticModels.size(); ++i)
	{
		const StaticModel& model = staticModels[i];
		painter->AddModel(model.material, model.geometry, model.transform);
	}

	for(size_t i = 0; i < rigidModels.size(); ++i)
	{
		const RigidModel& model = rigidModels[i];
		painter->AddModel(model.material, model.geometry, model.rigidBody->GetTransform());
	}

	for(size_t i = 0; i < staticLights.size(); ++i)
	{
		ptr<StaticLight> light = staticLights[i];
		if(light->shadow)
			painter->AddShadowLight(light->position, light->color, light->transform);
		else
			painter->AddBasicLight(light->position, light->color);
	}

#if 0
	//painter->SetAmbientColor(float3(0.2f, 0.2f, 0.2f));
	for(size_t i = 0; i < cubes.size(); ++i)
		painter->AddModel(texturedMaterial, geometryCube, CreateScalingMatrix(cubes[i].scale) * cubes[i].rigidBody->GetTransform());
	//painter->AddModel(zombieMaterial, geometryKnot, CreateScalingMatrix(0.3f, 0.3f, 0.3f) * CreateTranslationMatrix(10, 10, 2));
	//painter->AddModel(zombieMaterial, geometryZombi, CreateTranslationMatrix(10, 10, 0));

	float intPart;
	if(!theTimePaused)
		theTime += frameTime;

	float animationTime = modf(theTime / 11, &intPart) * 11;
	//bafZombi->Setup(float3(10, 10, 0), quaternion(0, 0, 0, 1), modf(theTime * 0.1f, &intPart));
	//bafZombi->Setup(float3(10, 15, 0), quaternion(0, 0, 0, 1), modf(theTime * 0.1f, &intPart) * 5);
	bafZombi->Setup(float3(10, 15, 0), quaternion(0, 0, 0, 1), animationTime);
	bafAxe->Setup(float3(10, 15, 0), quaternion(0, 0, 0, 1), animationTime);
	//bafZombi->Setup(float3(10, 15, 0), quaternion(float3(0, 0, 1), modf(theTime * 0.1f, &intPart)), 0);

//		for(size_t i = 0; i < bafZombi->animationWorldPositions.size(); ++i)
//			painter->AddModel(texturedMaterial, geometryCube, (float4x4)bafZombi->orientations[i] * CreateScalingMatrix(0.1f, 0.1f, 0.1f) * CreateTranslationMatrix(bafZombi->animationWorldPositions[i]));

	//bafZombi->Setup(float3(10, 10, 1), quaternion(0, 0, 0, 1), modf(t * 0.1f, &intPart));
	//bafZombi->Setup(float3(10, 10, 1), quaternion(float3(1, 0, 0), modf(t / 3, &intPart) * 3), 0);
	//bafZombi->Setup(float3(10, 10, 1), quaternion(0, 0, 0, 1), 0);
	//bafZombi->orientations[2] = quaternion(float3(1, 0, 0), modf(t / 3, &intPart)) * bafZombi->orientations[2];
	painter->AddSkinnedModel(zombieMaterial, geometryZombi, bafZombi);
	painter->AddModel(zombieMaterial, geometryAxe, (float4x4)bafAxe->animationWorldOrientations[0] * CreateTranslationMatrix(bafAxe->animationWorldPositions[0]));
#endif

	painter->SetupPostprocess(bloomLimit, toneLuminanceKey, toneMaxLuminance);

	painter->Draw();

	presenter->Present();
}

ptr<Game> Game::Get()
{
	return singleGame;
}

ptr<Texture> Game::LoadTexture(const String& fileName)
{
	return device->CreateStaticTexture(fileSystem->LoadFile(fileName));
}

ptr<Geometry> Game::LoadGeometry(const String& fileName)
{
	return NEW(Geometry(
		device->CreateVertexBuffer(fileSystem->LoadFile(fileName + ".vertices"), layout),
		device->CreateIndexBuffer(fileSystem->LoadFile(fileName + ".indices"), sizeof(short))
	));
}

ptr<Geometry> Game::LoadSkinnedGeometry(const String& fileName)
{
	return NEW(Geometry(
		device->CreateVertexBuffer(fileSystem->LoadFile(fileName + ".vertices"), skinnedLayout),
		device->CreateIndexBuffer(fileSystem->LoadFile(fileName + ".indices"), sizeof(short))
	));
}

ptr<Skeleton> Game::LoadSkeleton(const String& fileName)
{
	return Skeleton::Deserialize(fileSystem->LoadFileAsStream(fileName));
}

ptr<BoneAnimation> Game::LoadBoneAnimation(const String& fileName, ptr<Skeleton> skeleton)
{
	return BoneAnimation::Deserialize(fileSystem->LoadFileAsStream(fileName), skeleton);
}

ptr<Physics::Shape> Game::CreatePhysicsBoxShape(float halfSizeX, float halfSizeY, float halfSizeZ)
{
	return physicsWorld->CreateBoxShape(float3(halfSizeX, halfSizeY, halfSizeZ));
}

ptr<Physics::RigidBody> Game::CreatePhysicsRigidBody(ptr<Physics::Shape> physicsShape, float mass, float x, float y, float z)
{
	return physicsWorld->CreateRigidBody(physicsShape, mass, CreateTranslationMatrix(x, y, z));
}

void Game::AddStaticModel(ptr<Geometry> geometry, ptr<Material> material, float x, float y, float z)
{
	StaticModel model;
	model.geometry = geometry;
	model.material = material;
	model.transform = CreateTranslationMatrix(x, y, z);
	staticModels.push_back(model);
}

void Game::AddRigidModel(ptr<Geometry> geometry, ptr<Material> material, ptr<Physics::RigidBody> physicsRigidBody)
{
	RigidModel model;
	model.geometry = geometry;
	model.material = material;
	model.rigidBody = physicsRigidBody;
	rigidModels.push_back(model);
}

ptr<StaticLight> Game::AddStaticLight()
{
	ptr<StaticLight> light = NEW(StaticLight());
	staticLights.push_back(light);
	return light;
}

//******* Game::StaticLight

SCRIPTABLE_MAP_BEGIN(StaticLight, Farsh.StaticLight);
	SCRIPTABLE_METHOD(StaticLight, SetPosition);
	SCRIPTABLE_METHOD(StaticLight, SetTarget);
	SCRIPTABLE_METHOD(StaticLight, SetProjection);
	SCRIPTABLE_METHOD(StaticLight, SetColor);
	SCRIPTABLE_METHOD(StaticLight, SetShadow);
SCRIPTABLE_MAP_END();

StaticLight::StaticLight() :
	position(-1, 0, 0), target(0, 0, 0), angle(3.1415926535897932f / 4), nearPlane(0.1f), farPlane(100.0f), color(1, 1, 1), shadow(false)
{
	UpdateTransform();
}

void StaticLight::UpdateTransform()
{
	transform = CreateLookAtMatrix(position, target, float3(0, 0, 1)) * CreateProjectionPerspectiveFovMatrix(angle, 1.0f, nearPlane, farPlane);
}

void StaticLight::SetPosition(float x, float y, float z)
{
	position = float3(x, y, z);
	UpdateTransform();
}

void StaticLight::SetTarget(float x, float y, float z)
{
	target = float3(x, y, z);
	UpdateTransform();
}

void StaticLight::SetProjection(float angle, float nearPlane, float farPlane)
{
	this->angle = angle * 3.1415926535897932f / 180;
	this->nearPlane = nearPlane;
	this->farPlane = farPlane;
	UpdateTransform();
}

void StaticLight::SetColor(float r, float g, float b)
{
	color = float3(r, g, b);
}

void StaticLight::SetShadow(bool shadow)
{
	this->shadow = shadow;
}
