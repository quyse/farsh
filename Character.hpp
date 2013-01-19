#ifndef ___FARSH_CHARACTER_HPP___
#define ___FARSH_CHARACTER_HPP___

#include "general.hpp"
#include "Painter.hpp"

class BoneAnimation;
class BoneAnimationFrame;

struct AnimationInterval
{
	float begin;
	float end;
};

struct CharacterInfo : public Object
{
	ptr<Geometry> geometry, shadowGeometry;
	ptr<Painter::Material> material;
	ptr<Physics::Shape> physicsShape;
	ptr<BoneAnimation> animation;
	std::vector<AnimationInterval> animationIntervals;
};

/// Класс персонажа (или врага).
class Character : public Object
{
protected:
	ptr<CharacterInfo> info;
	ptr<BoneAnimationFrame> animationFrame;
	ptr<Physics::Character> physicsCharacter;

	/// Референсное положение.
	/** То, с которым рисуется модель. */
	float3 referencePosition;
	/// Направление персонажа.
	float alpha;
	/// Текущая анимация.
	int aiNumber;
	/// Время в текущей анимации.
	float animationTime;

	/// Выбрать следующий анимационный интервал.
	virtual void SelectAnimationInterval() = 0;
	/// Выполнить работу.
	virtual void Process() = 0;

public:
	Character(ptr<CharacterInfo> info, ptr<Physics::World> physicsWorld, const float3& position);

	float3 GetPhysicalPosition() const;

	void Tick(float time);
	void Draw(Painter* painter);
};

/// Класс зомби.
class Zombie : public Character
{
private:
	enum AI
	{
		aiRun,
		aiAttack
	};
	ptr<Character> target;

	static const float attackLength;

	bool CanAttack() const;

protected:
	void SelectAnimationInterval();
	void Process();

public:
	Zombie(ptr<CharacterInfo> info, ptr<Physics::World> physicsWorld, const float3& position, ptr<Character> target);
};

/// Класс ГГ.
class Hero : public Character
{
private:
	enum AI
	{
		aiStop,
		aiRun,
		aiRunBack
	};

	/// Вертикальный угол (для камеры).
	float beta;

	/// Текущее управление.
	float controlAlpha, controlBeta;
	bool controlMoveLeft, controlMoveRight;

	float4x4 cameraViewTransform;

protected:
	void SelectAnimationInterval();
	void Process();

public:
	Hero(ptr<CharacterInfo> info, ptr<Physics::World> physicsWorld, const float3& position);

	void Control(float controlAlpha, float controlBeta, bool controlMoveLeft, bool controlMoveRight);

	const float4x4& GetCameraViewTransform() const;
};

#endif
