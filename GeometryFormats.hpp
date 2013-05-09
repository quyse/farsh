#ifndef ___FARSH_GEOMETRY_FORMATS_HPP___
#define ___FARSH_GEOMETRY_FORMATS_HPP___

#include "general.hpp"

class GeometryFormats : public Object
{
public:
	//*** Обычные модели.
	ptr<VertexLayout> vl;
	ptr<AttributeLayout> al;
	ptr<AttributeLayoutSlot> als;
	ptr<AttributeLayoutElement> alePosition;
	ptr<AttributeLayoutElement> aleNormal;
	ptr<AttributeLayoutElement> aleTexcoord;
	//*** Skinned-модели.
	ptr<VertexLayout> vlSkinned;
	ptr<AttributeLayout> alSkinned;
	ptr<AttributeLayoutSlot> alsSkinned;
	ptr<AttributeLayoutElement> aleSkinnedPosition;
	ptr<AttributeLayoutElement> aleSkinnedNormal;
	ptr<AttributeLayoutElement> aleSkinnedTexcoord;
	ptr<AttributeLayoutElement> aleSkinnedBoneNumbers;
	ptr<AttributeLayoutElement> aleSkinnedBoneWeights;

	GeometryFormats();
};

#endif
