#include "BoneAnimation.hpp"
#include "Skeleton.hpp"
#include <iostream>

/*
Формат файла костной анимации:

Количество костей.
Количество ключей.
Ключ
{
	время (1 float)
	номер кости
	относительная ориентация (кватернион)
	если номер кости был 0,
		смещение (3 float'а)
}
*/

//*** BoneAnimation

BoneAnimation::BoneAnimation(ptr<Skeleton> skeleton, const std::vector<std::vector<Key> >& keys, const std::vector<float3>& rootBoneOffsets)
: skeleton(skeleton), keys(keys), rootBoneOffsets(rootBoneOffsets) {}

ptr<BoneAnimation> BoneAnimation::Deserialize(ptr<InputStream> inputStream, ptr<Skeleton> skeleton)
{
	try
	{
		StreamReader reader(inputStream);

		// количество костей
		size_t bonesCount = reader.ReadShortly();
		// проверить, что совпадает с количеством в скелете
		if(bonesCount != skeleton->GetBones().size())
			THROW_PRIMARY_EXCEPTION("Bones count is not equal to skeleton bones count");

		std::vector<std::vector<Key> > keys(bonesCount);

		// общее количество ключей
		size_t allKeysCount = reader.ReadShortly();

		// для корневой кости особые ключи
		struct RootKey
		{
			Key key;
			float3 offset;
			RootKey(const Key& key, const float3& offset) : key(key), offset(offset) {}
		};
		std::vector<RootKey> rootKeys;

		// считать ключи и распихать их по костям
		for(size_t i = 0; i < allKeysCount; ++i)
		{
			float keyTime = reader.Read<float>();
			int keyBone = reader.ReadShortly();
			quaternion keyOrientation = reader.Read<quaternion>();

			Key key;
			key.time = keyTime;
			key.orientation = keyOrientation;
			if(keyBone)
				keys[keyBone].push_back(key);
			else
				rootKeys.push_back(RootKey(key, reader.Read<float3>()));
		}

		// отсортировать ключи по времени
		struct KeySorter
		{
			bool operator()(const Key& a, const Key& b) const
			{
				return a.time < b.time;
			}
			bool operator()(const RootKey& a, const RootKey& b) const
			{
				return a.key.time < b.key.time;
			}
		} keySorter;
		std::sort(rootKeys.begin(), rootKeys.end(), keySorter);
		for(size_t i = 1; i < keys.size(); ++i)
		{
			std::vector<Key>& boneKeys = keys[i];
			std::sort(boneKeys.begin(), boneKeys.end(), keySorter);
		}

		// перенести корневые ключи на место
		keys[0].reserve(rootKeys.size());
		std::vector<float3> rootBoneOffsets;
		rootBoneOffsets.reserve(rootKeys.size());
		for(size_t i = 0; i < rootKeys.size(); ++i)
		{
			keys[0].push_back(rootKeys[i].key);
			rootBoneOffsets.push_back(rootKeys[i].offset);
		}

		return NEW(BoneAnimation(skeleton, keys, rootBoneOffsets));
	}
	catch(Exception* exception)
	{
		THROW_SECONDARY_EXCEPTION("Can't deserialize bone animation", exception);
	}
}

//*** BoneAnimationFrame

BoneAnimationFrame::BoneAnimationFrame(ptr<BoneAnimation> animation)
: animation(animation),
	animationRelativeOrientations(animation->keys.size()),
	animationWorldOrientations(animationRelativeOrientations.size()),
	animationWorldPositions(animationRelativeOrientations.size()),
	orientations(animationRelativeOrientations.size()),
	offsets(animationRelativeOrientations.size())
{}

template <int n>
std::ostream& operator<<(std::ostream& s, vector<n> v)
{
	for(int i = 0; i < n; ++i)
	{
		if(i) s << ' ';
		s << v.t[i];
	}
	return s;
}

void BoneAnimationFrame::Setup(const float3& originOffset, const quaternion& originOrientation, float time)
{
	// получить ключи и смещения
	const std::vector<std::vector<BoneAnimation::Key> >& keys = animation->keys;
	const std::vector<float3>& rootBoneOffsets = animation->rootBoneOffsets;

	struct Sorter
	{
		bool operator()(const BoneAnimation::Key& a, const BoneAnimation::Key& b) const
		{
			return a.time < b.time;
		}
	} sorter;

	int bonesCount = (int)animationRelativeOrientations.size();

	//std::cout << "Time: " << time << '\n';

	// для каждой кости получить анимационную относительную ориентацию
	// а для корневой кости получить ещё и позицию
	float3 rootBoneOffset;
	for(int i = 0; i < bonesCount; ++i)
	{
		const std::vector<BoneAnimation::Key>& boneKeys = keys[i];
		quaternion& animationRelativeOrientation = animationRelativeOrientations[i];

		// найти бинарным поиском следующий за временем ключ
		BoneAnimation::Key timeKey;
		timeKey.time = time;
		ptrdiff_t frame = std::upper_bound(boneKeys.begin(), boneKeys.end(), timeKey, sorter) - boneKeys.begin();
		float interframeTime;
		if(frame <= 0)
			animationRelativeOrientation = boneKeys.front().orientation;
		else if(frame >= (int)boneKeys.size())
			animationRelativeOrientation = boneKeys.back().orientation;
		else
		{
			interframeTime = (time - boneKeys[frame - 1].time) / (boneKeys[frame].time - boneKeys[frame - 1].time);
			animationRelativeOrientation = slerp(boneKeys[frame - 1].orientation, boneKeys[frame].orientation, interframeTime);
		}

		//std::cout << "ARO " << i << ": " << animationRelativeOrientation << '\n';

		// для корневой кости
		if(i == 0)
		{
			if(frame <= 0)
				rootBoneOffset = rootBoneOffsets.front();
			else if(frame >= (int)boneKeys.size())
				rootBoneOffset = rootBoneOffsets.back();
			else
				rootBoneOffset = lerp(rootBoneOffsets[frame - 1], rootBoneOffsets[frame], interframeTime);
		}
	}

	// вычислить анимационные мировые ориентации и позиции
	const std::vector<Skeleton::Bone>& bones = animation->skeleton->GetBones();
	const std::vector<int>& sortedBones = animation->skeleton->GetSortedBones();
	std::vector<bool> f(bonesCount);
	for(int i = 0; i < bonesCount; ++i)
	{
		int boneNumber = sortedBones[i];
		const Skeleton::Bone& bone = bones[boneNumber];
		if(boneNumber)
		{
			int parent = bone.parent;
			if(!f[parent])
				THROW_PRIMARY_EXCEPTION("Parent is not calculated");
			animationWorldOrientations[boneNumber] = animationRelativeOrientations[boneNumber] * animationWorldOrientations[parent];
			animationWorldPositions[boneNumber] = animationWorldPositions[parent] + bone.originalRelativePosition * animationWorldOrientations[parent];
//			orientations[boneNumber] = dynamicOrientations[boneNumber] * orientations[parent];
//			offsets[boneNumber] = offsets[parent] + bone.originalOffset * orientations[parent];
		}
		else
		{
//			orientations[boneNumber] = dynamicOrientations[boneNumber] * originOrientation;
//			offsets[boneNumber] = originOffset + rootBoneOffset * originOrientation;
			animationWorldOrientations[boneNumber] = animationRelativeOrientations[boneNumber] * originOrientation;
			animationWorldPositions[boneNumber] = originOffset + rootBoneOffset * originOrientation;
		}
		f[boneNumber] = true;
		//std::cout << "Bone " << boneNumber << ", AWO=" << animationWorldOrientations[boneNumber] << ", AWP=" << animationWorldPositions[boneNumber] << '\n';
	}

	// вычислить результирующие ориентации для костей
	for(int i = 0; i < bonesCount; ++i)
	{
//		orientations[i] = bones[i].originalWorldOrientation.conjugate() * animationWorldOrientations[i];
//		offsets[i] = animationWorldPositions[i] - bones[i].originalWorldPosition * orientations[i];

#if 0
		// это типа неправильно
		orientations[i] = bones[i].originalWorldOrientation.conjugate() * animationWorldOrientations[i];
		offsets[i] = animationWorldPositions[i] - bones[i].originalWorldPosition * (bones[i].originalWorldOrientation.conjugate() * animationWorldOrientations[i]);
#else
		// это типа правильно
		orientations[i] = bones[i].originalWorldOrientation * animationWorldOrientations[i].conjugate();
		offsets[i] = animationWorldPositions[i] - bones[i].originalWorldPosition * orientations[i];
#endif
		//std::cout << "Result " << i << " O=" << orientations[i] << ", P=" << offsets[i] << '\n';
	}
	//std::cout << '\n';
	//std::swap(orientations[1], orientations[2]);
	//std::swap(offsets[1], offsets[2]);
}
