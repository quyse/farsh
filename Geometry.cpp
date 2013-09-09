#include "Geometry.hpp"

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
