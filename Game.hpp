#ifndef ___FARSH_GAME_HPP___
#define ___FARSH_GAME_HPP___

#include "general.hpp"

class Geometry;
class GeometryFormats;
struct Material;
class Skeleton;
class BoneAnimation;
class BoneAnimationFrame;
class Painter;

struct StaticLight : public Object
{
	vec3 position;
	vec3 target;
	float angle;
	float nearPlane, farPlane;
	vec3 color;
	bool shadow;
	mat4x4 transform;

	StaticLight();

	void UpdateTransform();

	//*** Методы для скрипта.
	void SetPosition(const vec3& position);
	void SetTarget(const vec3& target);
	void SetProjection(float angle, float nearPlane, float farPlane);
	void SetColor(const vec3& color);
	void SetShadow(bool shadow);

	META_DECLARE_CLASS(StaticLight);
};

/// Класс игры.
class Game : public Object
{
private:
	ptr<Platform::Window> window;
	ptr<Device> device;
	ptr<Context> context;
	ptr<Presenter> presenter;

	ptr<GeometryFormats> geometryFormats;

	ptr<Painter> painter;

	ptr<FileSystem> fileSystem;

	ptr<Input::Manager> inputManager;

	ptr<TextureManager> textureManager;
	ptr<Gui::GrCanvas> canvas;
	ptr<Gui::Font> font;

	float alpha;

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
		mat4x4 transform;
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

	int screenWidth, screenHeight;

	Ticker ticker;

	float cameraAlpha, cameraBeta;

	ptr<Physics::World> physicsWorld;
	struct Cube
	{
		ptr<Physics::RigidBody> rigidBody;
		vec3 scale;
		Cube(ptr<Physics::RigidBody> rigidBody, const vec3& scale = vec3(1, 1, 1))
		: rigidBody(rigidBody), scale(scale) {}
	};
	std::vector<Cube> cubes;

	vec3 ambientColor;

	float bloomLimit, toneLuminanceKey, toneMaxLuminance;

	/// Скрипт.
	ptr<Script::State> scriptState;
	/// Единственный экземпляр для игры.
	static Game* singleGame;

public:
	Game();

	void Run();
	void Tick();

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

	void SetAmbient(float r, float g, float b);
	void SetZombieParams(ptr<Material> material, ptr<Geometry> geometry, ptr<Skeleton> skeleton, ptr<BoneAnimation> animation);
	void SetHeroParams(ptr<Material> material, ptr<Geometry> geometry, ptr<Skeleton> skeleton, ptr<BoneAnimation> animation);
	void SetAxeParams(ptr<Material> material, ptr<Geometry> geometry, ptr<BoneAnimation> animation);
	void SetCircularParams(ptr<Material> material, ptr<Geometry> geometry, ptr<BoneAnimation> animation);

	void PlaceHero(float x, float y, float z);

	META_DECLARE_CLASS(Game);
};

#endif
