#include "Skeleton.hpp"
#include <stack>

/*
Формат файла скелета:

Трансформация - это кватернион (xyzw) плюс смещение.
0 кость - корневая.

Количество костей.
Кость
{
	номер родительской кости
	оригинальная трансформация
}
*/

Skeleton::Skeleton(const std::vector<Bone>& bones) : bones(bones)
{
	// отсортировать кости топологически
	sortedBones.reserve(bones.size());
	std::vector<bool> f(bones.size());

	// DFS без рекурсии
	std::stack<int> s;
	int bonesCount = (int)bones.size();
	for(int i = 0; i < bonesCount; ++i)
	{
		while(i >= 0 && !f[i])
		{
			f[i] = true;
			s.push(i);
			i = bones[i].parent;
		}

		while(!s.empty())
		{
			sortedBones.push_back(s.top());
			s.pop();
		}
	}
}

const std::vector<Skeleton::Bone>& Skeleton::GetBones() const
{
	return bones;
}

const std::vector<int>& Skeleton::GetSortedBones() const
{
	return sortedBones;
}

ptr<Skeleton> Skeleton::Deserialize(ptr<InputStream> inputStream)
{
	try
	{
		StreamReader reader(inputStream);

		// считать количество костей
		size_t bonesCount = reader.ReadShortly();
		std::vector<Bone> bones(bonesCount);
		// считать кости
		for(size_t i = 0; i < bonesCount; ++i)
		{
			bones[i].parent = (int)reader.ReadShortly();
			bones[i].originalWorldOrientation = reader.Read<quat>();
			bones[i].originalWorldPosition = reader.Read<vec3>();
		}
		bones[0].originalRelativePosition = bones[0].originalWorldPosition;
		for(size_t i = 1; i < bonesCount; ++i)
			bones[i].originalRelativePosition =
				fromEigen(
					toEigenQuat(bones[bones[i].parent].originalWorldOrientation).conjugate()
					* (toEigen(bones[i].originalWorldPosition) - toEigen(bones[bones[i].parent].originalWorldPosition))
				);

		return NEW(Skeleton(bones));
	}
	catch(Exception* exception)
	{
		THROW_SECONDARY("Can't deserialize skeleton", exception);
	}
}
