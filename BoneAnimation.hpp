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
		quat orientation;
	};

private:
	ptr<Skeleton> skeleton;

	/// Ключи анимации по костям, отсортированные по времени.
	std::vector<std::vector<Key> > keys;

	/// Смещения корневой кости (по времени соответствуют ключам анимации).
	std::vector<vec3> rootBoneOffsets;

public:
	BoneAnimation(ptr<Skeleton> skeleton, const std::vector<std::vector<Key> >& keys, const std::vector<vec3>& rootBoneOffsets);

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
	std::vector<quat> animationRelativeOrientations;
	/// Анимационные мировые ориентации.
	std::vector<quat> animationWorldOrientations;
	/// Анимационные мировые позиции.
	std::vector<vec3> animationWorldPositions;

public:
	/// Результирующие преобразования для точек.
	std::vector<quat> orientations;
	/// Результирующие смещения для точек.
	std::vector<vec3> offsets;

public:
	BoneAnimationFrame(ptr<BoneAnimation> animation);

	/// Установить параметры и рассчитать положение.
	void Setup(const vec3& originOffset, const quat& originOrientation, float time);
};

#endif
