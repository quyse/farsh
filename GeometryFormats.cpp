#include "GeometryFormats.hpp"

GeometryFormats::GeometryFormats() :

	vl(NEW(VertexLayout(32))),
	al(NEW(AttributeLayout())),
	als(al->AddSlot()),
	alePosition(al->AddElement(als, vl->AddElement(DataTypes::_vec3, 0))),
	aleNormal(al->AddElement(als, vl->AddElement(DataTypes::_vec3, 12))),
	aleTexcoord(al->AddElement(als, vl->AddElement(DataTypes::_vec2, 24))),

	vlSkinned(NEW(VertexLayout(52))),
	alSkinned(NEW(AttributeLayout())),
	alsSkinned(alSkinned->AddSlot()),
	aleSkinnedPosition(alSkinned->AddElement(alsSkinned, vlSkinned->AddElement(DataTypes::_vec3, 0))),
	aleSkinnedNormal(alSkinned->AddElement(alsSkinned, vlSkinned->AddElement(DataTypes::_vec3, 12))),
	aleSkinnedTexcoord(alSkinned->AddElement(alsSkinned, vlSkinned->AddElement(DataTypes::_vec2, 24))),
	aleSkinnedBoneNumbers(alSkinned->AddElement(alsSkinned, vlSkinned->AddElement(DataTypes::_uvec4, LayoutDataTypes::Uint8, 32))),
	aleSkinnedBoneWeights(alSkinned->AddElement(alsSkinned, vlSkinned->AddElement(DataTypes::_vec4, 36)))
{}
