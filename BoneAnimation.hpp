#ifndef ___FARSH_BONE_ANIMATION_HPP___
#define ___FARSH_BONE_ANIMATION_HPP___

#include "general.hpp"

class Skeleton;
class BoneAnimationFrame;

/// Класс анимации костей.
class BoneAnimation : public Object
{
	friend class BoneAnimationFrame;
public:
	/// Структура ключа анимации.
	struct Key
	{
		/// Время ключа анимации.
		float time;
		/// Ориентация относительно родительской кости.
		quaternion orientation;
	};

private:
	ptr<Skeleton> skeleton;

	/// Ключи анимации по костям, отсортированные по времени.
	std::vector<std::vector<Key> > keys;

	/// Смещения корневой кости (по времени соответствуют ключам анимации).
	std::vector<float3> rootBoneOffsets;

public:
	BoneAnimation(ptr<Skeleton> skeleton, const std::vector<std::vector<Key> >& keys, const std::vector<float3>& rootBoneOffsets);

	static ptr<BoneAnimation> Deserialize(ptr<InputStream> inputStream, ptr<Skeleton> skeleton);

	SCRIPTABLE_CLASS(BoneAnimation);
};

/// Класс кадра анимации костей.
/** Позволяет выставлять нужный кадр анимации и получать трансформации. */
class BoneAnimationFrame : public Object
{
public:
	ptr<BoneAnimation> animation;

	/// Анимационные относительные ориентации.
	std::vector<quaternion> animationRelativeOrientations;
	/// Анимационные мировые ориентации.
	std::vector<quaternion> animationWorldOrientations;
	/// Анимационные мировые позиции.
	std::vector<float3> animationWorldPositions;

public:
	/// Результирующие преобразования для точек.
	std::vector<quaternion> orientations;
	/// Результирующие смещения для точек.
	std::vector<float3> offsets;

public:
	BoneAnimationFrame(ptr<BoneAnimation> animation);

	/// Установить параметры и рассчитать положение.
	void Setup(const float3& originOffset, const quaternion& originOrientation, float time);
};

#endif
