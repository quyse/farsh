#include "GeometryFormats.hpp"

GeometryFormats::GeometryFormats() :

	vl(NEW(VertexLayout(32))),
	al(NEW(AttributeLayout())),
	als(al->AddSlot()),
	alePosition(al->AddElement(als, vl->AddElement(DataTypes::Float3, 0))),
	aleNormal(al->AddElement(als, vl->AddElement(DataTypes::Float3, 12))),
	aleTexcoord(al->AddElement(als, vl->AddElement(DataTypes::Float2, 24))),

	vlSkinned(NEW(VertexLayout(64))),
	alSkinned(NEW(AttributeLayout())),
	alsSkinned(alSkinned->AddSlot()),
	aleSkinnedPosition(alSkinned->AddElement(alsSkinned, vlSkinned->AddElement(DataTypes::Float3, 0))),
	aleSkinnedNormal(alSkinned->AddElement(alsSkinned, vlSkinned->AddElement(DataTypes::Float3, 12))),
	aleSkinnedTexcoord(alSkinned->AddElement(alsSkinned, vlSkinned->AddElement(DataTypes::Float2, 24))),
	aleSkinnedBoneNumbers(alSkinned->AddElement(alsSkinned, vlSkinned->AddElement(DataTypes::UInt4, 32))),
	aleSkinnedBoneWeights(alSkinned->AddElement(alsSkinned, vlSkinned->AddElement(DataTypes::Float4, 48)))
{}
