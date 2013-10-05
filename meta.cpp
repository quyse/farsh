#include "../inanity/script/lua/impl.ipp"
#include "../inanity/inanity-base-meta.ipp"
#include "../inanity/inanity-graphics-meta.ipp"
#include "../inanity/inanity-physics-meta.ipp"

#include "BoneAnimation.hpp"
#include "Game.hpp"
#include "Material.hpp"
#include "Skeleton.hpp"
#include "Geometry.hpp"

META_CLASS(BoneAnimation, Farsh.BoneAnimation);
META_CLASS_END();

META_CLASS(Game, Farsh.Game);
	META_STATIC_METHOD(Get);
	META_METHOD(LoadTexture);
	META_METHOD(LoadGeometry);
	META_METHOD(LoadSkinnedGeometry);
	META_METHOD(LoadSkeleton);
	META_METHOD(LoadBoneAnimation);
	META_METHOD(CreatePhysicsBoxShape);
	META_METHOD(CreatePhysicsRigidBody);
	META_METHOD(AddStaticModel);
	META_METHOD(AddRigidModel);
	META_METHOD(AddStaticLight);
	META_METHOD(SetDecalMaterial);
	META_METHOD(SetAmbient);
	META_METHOD(SetZombieParams);
	META_METHOD(SetHeroParams);
	META_METHOD(SetAxeParams);
	META_METHOD(SetCircularParams);
	META_METHOD(PlaceHero);
META_CLASS_END();

META_CLASS(StaticLight, Farsh.StaticLight);
	META_METHOD(SetPosition);
	META_METHOD(SetTarget);
	META_METHOD(SetProjection);
	META_METHOD(SetColor);
	META_METHOD(SetShadow);
META_CLASS_END();

META_CLASS(Material, Farsh.Material);
	META_CONSTRUCTOR();
	META_METHOD(SetDiffuseTexture);
	META_METHOD(SetSpecularTexture);
	META_METHOD(SetNormalTexture);
	META_METHOD(SetDiffuse);
	META_METHOD(SetSpecular);
	META_METHOD(SetNormalCoordTransform);
	META_METHOD(SetEnvironmentCoef);
META_CLASS_END();

META_CLASS(Skeleton, Farsh.Skeleton);
META_CLASS_END();

META_CLASS(Geometry, Farsh.Geometry);
META_CLASS_END();
