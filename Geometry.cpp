#include "Geometry.hpp"

META_CLASS(Geometry, Farsh.Geometry);
META_CLASS_END();

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
