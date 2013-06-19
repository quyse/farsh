#ifndef ___FARSH_GEOMETRY_HPP___
#define ___FARSH_GEOMETRY_HPP___

#include "general.hpp"

class Geometry : public Object
{
private:
	ptr<VertexBuffer> vertexBuffer;
	ptr<IndexBuffer> indexBuffer;

public:
	Geometry(ptr<VertexBuffer> vertexBuffer, ptr<IndexBuffer> indexBuffer);

	ptr<VertexBuffer> GetVertexBuffer() const;
	ptr<IndexBuffer> GetIndexBuffer() const;

	META_DECLARE_CLASS(Geometry);
};

#endif
