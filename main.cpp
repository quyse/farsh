#include "general.hpp"
#include "Painter.hpp"
#include <sstream>
#include <iostream>
#include <fstream>

#define TEST_GRAPHICS_DIRECTX
//#define TEST_GRAPHICS_OPENGL

#include "test.hpp"

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

	ptr<Input::Manager> inputManager;

	float alpha;

	ptr<VertexBuffer> vertexBuffer;
	ptr<IndexBuffer> indexBuffer;
	ptr<Texture> diffuseTexture;
	ptr<Texture> specularTexture;

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

public:
	Game() : lastTick(0), cameraAlpha(0), cameraBeta(0)
	{
		tickCoef = 1.0f / Time::GetTicksPerSecond();
	}

	void onTick(int)
	{
		long long tick = Time::GetTicks();
		float frameTime = lastTick ? (tick - lastTick) * tickCoef : 0;
		lastTick = tick;

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
					case 90:
						{
							ptr<Physics::RigidBody> rigidBody = physicsWorld->CreateRigidBody(cubePhysicsShape, 100, physicsCharacter->GetTransform());
							rigidBody->ApplyImpulse(float3(cos(cameraAlpha) * cos(cameraBeta), sin(cameraAlpha) * cos(cameraBeta), sin(cameraBeta)) * 1000);
							cubes.push_back(rigidBody);
						}
						break;
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
		float cameraStep = 10;
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

		float3 shadowLightPosition = cameraPosition;
		float4x4 shadowLightTransform = CreateLookAtMatrix(shadowLightPosition, shadowLightPosition + cameraDirection, float3(0, 0, 1)) * CreateProjectionPerspectiveFovMatrix(3.1415926535897932f / 4, 1, 1, 100);
		//float3 shadowLightPosition2 = cameraPosition + cameraRightDirection * -1;
		//float4x4 shadowLightTransform2 = CreateLookAtMatrix(shadowLightPosition2, shadowLightPosition2 + cameraDirection, float3(0, 0, 1)) * CreateProjectionPerspectiveFovMatrix(3.1415926535897932f / 4, 1, 1, 100);
		float3 shadowLightPosition2(-10, -20, 20);
		float4x4 shadowLightTransform2 = CreateLookAtMatrix(shadowLightPosition2, float3(0, 0, 0), float3(0, 0, 1)) * CreateProjectionPerspectiveFovMatrix(3.1415926535897932f / 4, 1, 1, 100);

		physicsWorld->Simulate(frameTime);

		// зарегистрировать все объекты
		painter->BeginFrame();
		for(size_t i = 0; i < cubes.size(); ++i)
			painter->AddModel(diffuseTexture, specularTexture, vertexBuffer, indexBuffer, CreateScalingMatrix(cubes[i].scale) * cubes[i].rigidBody->GetTransform());

		painter->DoShadowPass(0, shadowLightTransform);
		painter->DoShadowPass(1, shadowLightTransform2);

		painter->BeginOpaque(viewMatrix * projMatrix, cameraPosition);
		painter->SetLightVariant(0, 2);
		painter->SetAmbientLight(float3(0, 0, 0));
		//painter->SetBasicLight(0, cameraPosition + cameraRightDirection * (-3.0f), float3(0.0f, 0.1f, 0.0f));
		//painter->SetBasicLight(1, cameraPosition + cameraRightDirection * (20.0f), float3(0.0f, 0.0f, 0.1f));
		painter->SetShadowLight(0, shadowLightPosition, float3(0.2f, 0.2f, 0.2f), shadowLightTransform);
		painter->SetShadowLight(1, shadowLightPosition2, float3(0.2f, 0.2f, 0.2f), shadowLightTransform2);
		painter->ApplyLight();

		painter->DrawOpaque();

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

#ifdef _DEBUG
		mode.width = 640;
		mode.height = 480;
		mode.fullscreen = false;
#else
		mode.width = GetSystemMetrics(SM_CXSCREEN);
		mode.height = GetSystemMetrics(SM_CYSCREEN);
		mode.fullscreen = true;
#endif
		mode.pixelFormat = PixelFormats::intR8G8B8A8;
		presenter = device->CreatePresenter(window->CreateOutput(), mode);

		context = device->GetContext();

		painter = NEW(Painter(device, context, presenter, mode.width, mode.height));

		// разметка
		std::vector<Layout::Element> layoutElements;
		layoutElements.push_back(Layout::Element(DataTypes::Float3, 0, 0));
		layoutElements.push_back(Layout::Element(DataTypes::Float3, 12, 1));
		layoutElements.push_back(Layout::Element(DataTypes::Float2, 24, 2));
		ptr<Layout> layout = NEW(Layout(layoutElements, sizeof(Vertex)));

		alpha = 0;

		ptr<FileSystem> fs = FolderFileSystem::GetNativeFileSystem();

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
//		drawingState.vertexBuffer = device->CreateVertexBuffer(fs->LoadFile("circular.geo.vertices"), layout);
//		drawingState.indexBuffer = device->CreateIndexBuffer(fs->LoadFile("circular.geo.indices"), layout);
		vertexBuffer = device->CreateVertexBuffer(fs->LoadFile("box.geo.vertices"), layout);
		indexBuffer = device->CreateIndexBuffer(fs->LoadFile("box.geo.indices"), layout);
		//vertexBuffer = device->CreateVertexBuffer(fs->LoadFile("statuya.geo.vertices"), layout);
		//indexBuffer = device->CreateIndexBuffer(fs->LoadFile("statuya.geo.indices"), layout);
#endif

		diffuseTexture = device->CreateStaticTexture(fs->LoadFile("diffuse.jpg"));
		specularTexture = device->CreateStaticTexture(fs->LoadFile("specular.jpg"));

		physicsWorld = NEW(Physics::BtWorld());

		// пол и кубики лабиринта
		{
			std::fstream f("labyrint.txt", std::ios::in);
			int n, m;
			f >> n >> m;

			float cellHalfSize = 10;

			//const float modelScale = 0.002f;
			const float modelScale = 1;

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
};

#ifdef _DEBUG
int main()
#else
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, INT)
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
