#include "Geometry.hpp"

SCRIPTABLE_MAP_BEGIN(Geometry, Farsh.Geometry);
SCRIPTABLE_MAP_END();

Geometry::Geometry(ptr<VertexBuffer> vertexBuffer, ptr<IndexBuffer> indexBuffer)
: vertexBuffer(vertexBuffer), indexBuffer(indexBuffer) {}

ptr<VertexBuffer> Geometry::GetVertexBuffer() const
{
	return vertexBuffer;
}

ptr<IndexBuffer> Geometry::GetIndexBuffer() const
{
	return indexBuffer;
}
