#ifndef ___FARSH_BONE_ANIMATION_HPP___
#define ___FARSH_BONE_ANIMATION_HPP___

#include "general.hpp"

class Skeleton;
class BoneAnimationFrame;

/// Класс анимации костей.
class BoneAnimation : public Object
{
	friend class BoneAnimationFrame;
private:
	ptr<Skeleton> skeleton;

	/// Структура ключа анимации.
	struct Key
	{
		float time;
		quaternion orientation;
	};
	/// Ключи анимации по костям, отсортированные по времени.
	std::vector<std::vector<Key> > keys;

	/// Смещения корневой кости (по времени соответствуют ключам анимации).
	std::vector<float3> rootBoneOffsets;

public:
	BoneAnimation(ptr<Skeleton> skeleton, const std::vector<std::vector<Key> >& keys, const std::vector<float3>& rootBoneOffsets);

	static ptr<BoneAnimation> Deserialize(ptr<InputStream> inputStream, ptr<Skeleton> skeleton);
};

/// Класс кадра анимации костей.
/** Позволяет выставлять нужный кадр анимации и получать трансформации. */
class BoneAnimationFrame : public Object
{
private:
	ptr<BoneAnimation> animation;

	/// Текущие динамические ориентации.
	std::vector<quaternion> dynamicOrientations;

public:
	/// Результирующие ориентации костей (в мировом пространстве).
	std::vector<quaternion> orientations;
	/// Результирующие смещения костей (в мировом пространстве).
	std::vector<float3> offsets;

public:
	BoneAnimationFrame(ptr<BoneAnimation> animation);

	/// Установить параметры и рассчитать положение.
	void Setup(const float3& originOffset, const quaternion& originOrientation, float time);
};

#endif
