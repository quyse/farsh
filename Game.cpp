#include "Game.hpp"
#include "Painter.hpp"
#include "Geometry.hpp"
#include "GeometryFormats.hpp"
#include "Material.hpp"
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
	SCRIPTABLE_METHOD(Game, SetDecalMaterial);
	SCRIPTABLE_METHOD(Game, SetZombieParams);
	SCRIPTABLE_METHOD(Game, SetHeroParams);
	SCRIPTABLE_METHOD(Game, SetAxeParams);
	SCRIPTABLE_METHOD(Game, SetCircularParams);
	SCRIPTABLE_METHOD(Game, PlaceHero);
SCRIPTABLE_MAP_END();

Game* Game::singleGame = 0;

const float Game::hzAFRun1 = 50.0f / 30;
const float Game::hzAFRun2 = 66.0f / 30;
const float Game::hzAFBattle1 = 400.0f / 30;
const float Game::hzAFBattle2 = 450.0f / 30;

Game::Game() :
	lastTick(0), cameraAlpha(0), cameraBeta(0),
	bloomLimit(10.0f), toneLuminanceKey(0.12f), toneMaxLuminance(3.1f),
	heroAnimationTime(hzAFBattle1)
{
	singleGame = this;

	tickCoef = 1.0f / Time::GetTicksPerSecond();
}

void Game::Run()
{
	try
	{
#ifdef FARSH_USE_DIRECTX11
		ptr<Graphics::System> system = NEW(Dx11System());
#endif
#ifdef FARSH_USE_OPENGL
		ptr<Graphics::System> system = NEW(GlSystem());
#endif

		device = system->CreatePrimaryDevice();
		ptr<Win32Window> window = system->CreateDefaultWindow().FastCast<Win32Window>();
		this->window = window;
		window->SetTitle("F.A.R.S.H.");

#ifdef ___INANITY_WINDOWS
		{
			ptr<Input::Win32Manager> im = NEW(Input::Win32RawManager(window->GetHWND()));
			inputManager = im;
			window->SetInputManager(im);
		}
#endif
#ifdef ___INANITY_LINUX
		inputManager = NEW(Input::X11Manager(window));
#endif

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
		ptr<ShaderCache> shaderCache = NEW(ShaderCache(NEW(SQLiteFileSystem(shadersCacheFileName)), device,
			system->CreateShaderCompiler(), system->CreateShaderGenerator(), NEW(Crypto::WhirlpoolStream())));

		fileSystem =
#ifdef PRODUCTION
			NEW(BlobFileSystem(FolderFileSystem::GetNativeFileSystem()->LoadFile("data")))
#else
			NEW(BufferedFileSystem(FolderFileSystem::GetNativeFileSystem()))
#endif
		;

		geometryFormats = NEW(GeometryFormats());

		painter = NEW(Painter(device, context, presenter, mode.width, mode.height, shaderCache, geometryFormats));

		textureManager = NEW(TextureManager(fileSystem, device));
		fontManager = NEW(FontManager(fileSystem, textureManager));
		textDrawer = TextDrawer::Create(device, shaderCache);
		font = fontManager->Get("mnogobukov.font");

		physicsWorld = NEW(Physics::BtWorld());

		// запустить стартовый скрипт
		scriptState = NEW(Lua::State());
		scriptState->RegisterClass<Game>();
		scriptState->RegisterClass<Material>();

		ptr<Script> mainScript = scriptState->LoadScript(fileSystem->LoadFile(
#ifdef PRODUCTION
			"main.luab"
#else
			"main.lua"
#endif
		));
		mainScript->Run<void>();

		try
		{
			window->Run(Win32Window::ActiveHandler::CreateDelegate(MakePointer(this), &Game::Tick));

			scriptState = 0;
		}
		catch(Exception* exception)
		{
			scriptState = 0;
			THROW_SECONDARY_EXCEPTION("Error while running game", exception);
		}
	}
	catch(Exception* exception)
	{
		THROW_SECONDARY_EXCEPTION("Can't initialize game", exception);
	}
}

void Game::Tick(int)
{
	long long tick = Time::GetTicks();
	float frameTime = lastTick ? (tick - lastTick) * tickCoef : 0;
	lastTick = tick;

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
					// TEST
#ifdef ___INANITY_TRACE_PTR
					managedHeap.PrintPtrs(std::cout);
#endif

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
						//rigidBody->ApplyImpulse(vec3(cos(cameraAlpha) * cos(cameraBeta), sin(cameraAlpha) * cos(cameraBeta), sin(cameraBeta)) * 1000);
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

	vec3 cameraDirection = vec3(cos(cameraAlpha) * cos(cameraBeta), sin(cameraAlpha) * cos(cameraBeta), sin(cameraBeta));
	vec3 cameraRightDirection = normalize(cross(cameraDirection, vec3(0, 0, 1)));
	vec3 cameraUpDirection = cross(cameraRightDirection, cameraDirection);

	const Input::State& inputState = inputFrame->GetCurrentState();
	/*
	left up right down Q E
	37 38 39 40
	65 87 68 83 81 69
	*/
	float cameraStep = 5;
	vec3 cameraMove(0, 0, 0);
	vec3 cameraMoveDirectionFront(cos(cameraAlpha), sin(cameraAlpha), 0);
	vec3 cameraMoveDirectionUp(0, 0, 1);
	vec3 cameraMoveDirectionRight = cross(cameraMoveDirectionFront, cameraMoveDirectionUp);
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

	//heroCharacter->Walk(cameraMove);

	physicsWorld->Simulate(frameTime);

	mat4x4 heroTransform = heroCharacter->GetTransform();
	vec3 heroPosition(heroTransform(0, 3), heroTransform(1, 3), heroTransform(2, 3));
	quat heroOrientation = axis_rotation(vec3(0, 0, 1), cameraAlpha);

	static vec3 cameraPosition(0, 0, 0);
	//vec3 cameraPosition = heroPosition - cameraMoveDirectionFront * 2.0f + cameraMoveDirectionUp * 2.0f;
	cameraPosition += cameraMove * frameTime;

	//std::cout << "cameraPosition = " << cameraPosition << '\n';

	context->Reset();

	alpha += frameTime;

	mat4x4 viewMatrix = CreateLookAtMatrix(cameraPosition, cameraPosition + cameraDirection, vec3(0, 0, 1));
#ifdef FARSH_USE_OPENGL
	viewMatrix = fromEigen((toEigen(viewMatrix) * Eigen::Scaling(Eigen::Vector3f(1, -1, 1))).eval().matrix());
#endif
	mat4x4 projMatrix = CreateProjectionPerspectiveFovMatrix(3.1415926535897932f / 4, float(mode.width) / float(mode.height), 0.1f, 100.0f);

	// зарегистрировать все объекты
	painter->BeginFrame(frameTime);
	painter->SetCamera(projMatrix * viewMatrix, cameraPosition);
	painter->SetAmbientColor(vec3(0, 0, 0));

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

	if(!theTimePaused)
		heroAnimationTime += frameTime;
	while(heroAnimationTime >= hzAFBattle2)
		heroAnimationTime += hzAFBattle1 - hzAFBattle2;

	// TEST: set time to zero
	//heroAnimationTime = 0;

	// TEST: rotate hero constantly
#if 1
	static float heroTime = 0;
	heroTime += frameTime;
	heroOrientation = axis_rotation(vec3(0, 0, 1), heroTime);
#endif

	heroAnimationFrame->Setup(heroPosition, heroOrientation, heroAnimationTime);
	//vec3 shouldBeHeroPosition = heroPosition - (heroAnimationFrame->animationWorldPositions[0] - heroPosition) * vec3(1, 1, 0);
	//heroAnimationFrame->Setup(shouldBeHeroPosition, heroOrientation, heroAnimationTime);
	painter->AddSkinnedModel(heroMaterial, heroGeometry, heroAnimationFrame);
	zombieAnimationFrame->Setup(heroPosition, heroOrientation, heroAnimationTime);
	painter->AddSkinnedModel(zombieMaterial, zombieGeometry, zombieAnimationFrame);
	if(0)
	for(size_t i = 0; i < heroAnimationFrame->animationWorldPositions.size(); ++i)
		painter->AddModel(
			staticModels[0].material,
			staticModels[0].geometry,
			fromEigen((
				Eigen::Translation3f(toEigen(heroAnimationFrame->animationWorldPositions[i])) *
				toEigenQuat(heroAnimationFrame->animationWorldOrientations[i]) *
				Eigen::Scaling(Eigen::Vector3f(0.1f, 0.1f, 0.1f))
			).matrix().eval())
		);
	circularAnimationFrame->Setup(heroPosition, heroOrientation, heroAnimationTime);
	painter->AddModel(circularMaterial, circularGeometry,
		fromEigen((
			Eigen::Translation3f(toEigen(circularAnimationFrame->animationWorldPositions[0])) *
			toEigenQuat(circularAnimationFrame->animationWorldOrientations[0])
		).matrix().eval())
	);
	axeAnimationFrame->Setup(heroPosition, heroOrientation, heroAnimationTime);
	painter->AddModel(axeMaterial, axeGeometry,
		fromEigen((
			Eigen::Translation3f(toEigen(axeAnimationFrame->animationWorldPositions[0])) *
			toEigenQuat(axeAnimationFrame->animationWorldOrientations[0])
		).matrix().eval())
	);

	// тестовая декаль
#if 0
	if(0)
	{
		mat4x4 transform = CreateLookAtMatrix(vec3(9, 10, 1), vec3(10, 10, 0), vec3(0, 0, 1))
			* CreateProjectionPerspectiveFovMatrix(3.1415926535897932f / 2, 1.0f, 0.1f, 10.0f);
		// полный отстой, но инвертирования матрицы пока нет
		D3DXMATRIX mxA((const float*)transform.t), mxB;
		D3DXMatrixInverse(&mxB, NULL, &mxA);
		mat4x4 c;
		c(0, 0) = mxB._11; c(0, 1) = mxB._12; c(0, 2) = mxB._13; c(0, 3) = mxB._14;
		c(1, 0) = mxB._21; c(1, 1) = mxB._22; c(1, 2) = mxB._23; c(1, 3) = mxB._24;
		c(2, 0) = mxB._31; c(2, 1) = mxB._32; c(2, 2) = mxB._33; c(2, 3) = mxB._34;
		c(3, 0) = mxB._41; c(3, 1) = mxB._42; c(3, 2) = mxB._43; c(3, 3) = mxB._44;
		painter->AddDecal(decalMaterial, transform, c);
	}
#endif

	painter->SetupPostprocess(bloomLimit, toneLuminanceKey, toneMaxLuminance);

	painter->Draw();

	textDrawer->Prepare(context);
	textDrawer->SetFont(font);

	// fps
	{
		static int tickCount = 0;
		static const int needTickCount = 100;
		static float allTicksTime = 0;
		allTicksTime += frameTime;
		static float lastAllTicksTime = 0;
		if(++tickCount >= needTickCount)
		{
			lastAllTicksTime = allTicksTime;
			allTicksTime = 0;
			tickCount = 0;
		}
		char fpsString[64];
		sprintf(fpsString, "frameTime: %.6f sec, FPS: %.6f\n", lastAllTicksTime / needTickCount, needTickCount / lastAllTicksTime);
		textDrawer->DrawTextLine(fpsString, -0.95f - 2.0f / context->GetTargetState().viewportWidth, -0.95f - 2.0f / context->GetTargetState().viewportHeight, vec4(1, 1, 1, 1), FontAlignments::Left | FontAlignments::Bottom);
		textDrawer->DrawTextLine(fpsString, -0.95f, -0.95f, vec4(1, 0, 0, 1), FontAlignments::Left | FontAlignments::Bottom);
	}

	textDrawer->Flush();

	presenter->Present();
}

ptr<Game> Game::Get()
{
	return singleGame;
}

ptr<Texture> Game::LoadTexture(const String& fileName)
{
	return textureManager->Get(fileName);
}

ptr<Geometry> Game::LoadGeometry(const String& fileName)
{
	return NEW(Geometry(
		device->CreateStaticVertexBuffer(fileSystem->LoadFile(fileName + ".vertices"), geometryFormats->vl),
		device->CreateStaticIndexBuffer(fileSystem->LoadFile(fileName + ".indices"), sizeof(short))
	));
}

ptr<Geometry> Game::LoadSkinnedGeometry(const String& fileName)
{
	return NEW(Geometry(
		device->CreateStaticVertexBuffer(fileSystem->LoadFile(fileName + ".vertices"), geometryFormats->vlSkinned),
		device->CreateStaticIndexBuffer(fileSystem->LoadFile(fileName + ".indices"), sizeof(short))
	));
}

ptr<Skeleton> Game::LoadSkeleton(const String& fileName)
{
	return Skeleton::Deserialize(fileSystem->LoadStream(fileName));
}

ptr<BoneAnimation> Game::LoadBoneAnimation(const String& fileName, ptr<Skeleton> skeleton)
{
	if(!skeleton)
	{
		std::vector<Skeleton::Bone> bones(1);
		bones[0].originalWorldPosition = vec3(0, 0, 0);
		bones[0].originalRelativePosition = vec3(0, 0, 0);
		bones[0].parent = 0;
		skeleton = NEW(Skeleton(bones));
	}
	return BoneAnimation::Deserialize(fileSystem->LoadStream(fileName), skeleton);
}

ptr<Physics::Shape> Game::CreatePhysicsBoxShape(float halfSizeX, float halfSizeY, float halfSizeZ)
{
	return physicsWorld->CreateBoxShape(vec3(halfSizeX, halfSizeY, halfSizeZ));
}

ptr<Physics::RigidBody> Game::CreatePhysicsRigidBody(ptr<Physics::Shape> physicsShape, float mass, float x, float y, float z)
{
	Eigen::Affine3f startTransform = Eigen::Affine3f::Identity();
	startTransform.translate(Eigen::Vector3f(x, y, z));
	return physicsWorld->CreateRigidBody(physicsShape, mass, fromEigen(startTransform.matrix()));
}

void Game::AddStaticModel(ptr<Geometry> geometry, ptr<Material> material, float x, float y, float z)
{
	StaticModel model;
	model.geometry = geometry;
	model.material = material;
	Eigen::Affine3f transform = Eigen::Affine3f::Identity();
	transform.translate(Eigen::Vector3f(x, y, z));
	model.transform = fromEigen(transform.matrix());
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

void Game::SetDecalMaterial(ptr<Material> decalMaterial)
{
	this->decalMaterial = decalMaterial;
}

void Game::SetZombieParams(ptr<Material> material, ptr<Geometry> geometry, ptr<Skeleton> skeleton, ptr<BoneAnimation> animation)
{
	this->zombieMaterial = material;
	this->zombieGeometry = geometry;
	this->zombieSkeleton = skeleton;
	this->zombieAnimation = animation;
}

void Game::SetHeroParams(ptr<Material> material, ptr<Geometry> geometry, ptr<Skeleton> skeleton, ptr<BoneAnimation> animation)
{
	this->heroMaterial = material;
	this->heroGeometry = geometry;
	this->heroSkeleton = skeleton;
	this->heroAnimation = animation;
}

void Game::SetAxeParams(ptr<Material> material, ptr<Geometry> geometry, ptr<BoneAnimation> animation)
{
	this->axeMaterial = material;
	this->axeGeometry = geometry;
	this->axeAnimation = animation;
}

void Game::SetCircularParams(ptr<Material> material, ptr<Geometry> geometry, ptr<BoneAnimation> animation)
{
	this->circularMaterial = material;
	this->circularGeometry = geometry;
	this->circularAnimation = animation;
}

void Game::PlaceHero(float x, float y, float z)
{
	Eigen::Affine3f startTransform = Eigen::Affine3f::Identity();
	startTransform.translate(Eigen::Vector3f(x, y, z));
	mat4x4 initialTransform = fromEigen(startTransform.matrix());
	heroCharacter = physicsWorld->CreateCharacter(physicsWorld->CreateCapsuleShape(0.2f, 1.4f), initialTransform);
	heroAnimationFrame = NEW(BoneAnimationFrame(heroAnimation));
	circularAnimationFrame = NEW(BoneAnimationFrame(circularAnimation));
	zombieAnimationFrame = NEW(BoneAnimationFrame(zombieAnimation));
	axeAnimationFrame = NEW(BoneAnimationFrame(axeAnimation));
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
	transform =
		CreateProjectionPerspectiveFovMatrix(angle, 1.0f, nearPlane, farPlane) *
		CreateLookAtMatrix(position, target, vec3(0, 0, 1));
}

void StaticLight::SetPosition(float x, float y, float z)
{
	position = vec3(x, y, z);
	UpdateTransform();
}

void StaticLight::SetTarget(float x, float y, float z)
{
	target = vec3(x, y, z);
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
	color = vec3(r, g, b);
}

void StaticLight::SetShadow(bool shadow)
{
	this->shadow = shadow;
}
