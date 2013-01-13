#ifndef ___FARSH_SKELETON_HPP___
#define ___FARSH_SKELETON_HPP___

#include "general.hpp"

/// Класс скелета.
/** Содержит иерархию костей. */
class Skeleton : public Object
{
public:
	/// Структура кости.
	struct Bone
	{
		/// Оригинальная ориентация.
		quaternion originalOrientation;
		/// Оригинальное смещение.
		float3 originalOffset;
		/// Номер родительской кости.
		int parent;
	};

private:
	/// Кости.
	std::vector<Bone> bones;
	/// Порядок топологической сортировки для костей.
	std::vector<int> sortedBones;

public:
	Skeleton(const std::vector<Bone>& bones);

	const std::vector<Bone>& GetBones() const;
	const std::vector<int>& GetSortedBones() const;

	static ptr<Skeleton> Deserialize(ptr<InputStream> inputStream);
};

#endif
