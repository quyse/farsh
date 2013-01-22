#ifndef ___FARSH_GAME_HPP___
#define ___FARSH_GAME_HPP___

#include "general.hpp"

struct Material;
class Skeleton;
class BoneAnimation;
class BoneAnimationFrame;
class Painter;

struct StaticLight : public Object
{
	float3 position;
	float3 target;
	float angle;
	float nearPlane, farPlane;
	float3 color;
	bool shadow;
	float4x4 transform;

	StaticLight();

	void UpdateTransform();

	//*** Методы для скрипта.
	void SetPosition(float x, float y, float z);
	void SetTarget(float x, float y, float z);
	void SetProjection(float angle, float nearPlane, float farPlane);
	void SetColor(float r, float g, float b);
	void SetShadow(bool shadow);

	SCRIPTABLE_CLASS(StaticLight);
};

/// Класс игры.
class Game : public Object
{
private:
	ptr<Window> window;
	ptr<Device> device;
	ptr<Context> context;
	ptr<Presenter> presenter;

	ptr<Painter> painter;

	ptr<FileSystem> fileSystem;

	ptr<Input::Manager> inputManager;

	float alpha;

	ptr<Layout> layout, skinnedLayout;

	ptr<Material> decalMaterial;

	ptr<Material> zombieMaterial;
	ptr<Geometry> zombieGeometry;
	ptr<Skeleton> zombieSkeleton;
	ptr<BoneAnimation> zombieAnimation;
	struct Zombie
	{
		ptr<Physics::Character> character;
		ptr<BoneAnimationFrame> animationFrame;
	};
	std::vector<Zombie> zombies;

	ptr<Material> heroMaterial;
	ptr<Geometry> heroGeometry;
	ptr<Skeleton> heroSkeleton;
	ptr<BoneAnimation> heroAnimation;
	// экземпляр героя
	ptr<Physics::Character> heroCharacter;
	ptr<BoneAnimationFrame> heroAnimationFrame;
	ptr<BoneAnimationFrame> circularAnimationFrame;
	float heroAnimationTime;
	// тестовый экземпляр зомби
	ptr<BoneAnimationFrame> zombieAnimationFrame;
	ptr<BoneAnimationFrame> axeAnimationFrame;

	static const float hzAFRun1, hzAFRun2, hzAFBattle1, hzAFBattle2;

	ptr<Material> axeMaterial;
	ptr<Geometry> axeGeometry;
	ptr<BoneAnimation> axeAnimation;

	ptr<Material> circularMaterial;
	ptr<Geometry> circularGeometry;
	ptr<BoneAnimation> circularAnimation;

	struct StaticModel
	{
		ptr<Geometry> geometry;
		ptr<Material> material;
		float4x4 transform;
	};
	std::vector<StaticModel> staticModels;

	struct RigidModel
	{
		ptr<Geometry> geometry;
		ptr<Material> material;
		ptr<Physics::RigidBody> rigidBody;
	};
	std::vector<RigidModel> rigidModels;

	std::vector<ptr<StaticLight> > staticLights;

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

	float bloomLimit, toneLuminanceKey, toneMaxLuminance;

	/// Скрипт.
	ptr<ScriptState> scriptState;
	/// Единственный экземпляр для игры.
	static Game* singleGame;

public:
	Game();

	void Run();
	void Tick(int);

	//******* Методы, доступные из скрипта.

	static ptr<Game> Get();

	ptr<Texture> LoadTexture(const String& fileName);
	ptr<Geometry> LoadGeometry(const String& fileName);
	ptr<Geometry> LoadSkinnedGeometry(const String& fileName);
	ptr<Skeleton> LoadSkeleton(const String& fileName);
	ptr<BoneAnimation> LoadBoneAnimation(const String& fileName, ptr<Skeleton> skeleton);
	ptr<Physics::Shape> CreatePhysicsBoxShape(float halfSizeX, float halfSizeY, float halfSizeZ);
	ptr<Physics::RigidBody> CreatePhysicsRigidBody(ptr<Physics::Shape> physicsShape, float mass, float x, float y, float z);
	void AddStaticModel(ptr<Geometry> geometry, ptr<Material> material, float x, float y, float z);
	void AddRigidModel(ptr<Geometry> geometry, ptr<Material> material, ptr<Physics::RigidBody> physicsRigidBody);
	ptr<StaticLight> AddStaticLight();
	void SetDecalMaterial(ptr<Material> decalMaterial);

	void SetZombieParams(ptr<Material> material, ptr<Geometry> geometry, ptr<Skeleton> skeleton, ptr<BoneAnimation> animation);
	void SetHeroParams(ptr<Material> material, ptr<Geometry> geometry, ptr<Skeleton> skeleton, ptr<BoneAnimation> animation);
	void SetAxeParams(ptr<Material> material, ptr<Geometry> geometry, ptr<BoneAnimation> animation);
	void SetCircularParams(ptr<Material> material, ptr<Geometry> geometry, ptr<BoneAnimation> animation);

	void PlaceHero(float x, float y, float z);

	SCRIPTABLE_CLASS(Game);
};

#endif
