#ifndef ___FARSH_SHADER_CACHE_HPP___
#define ___FARSH_SHADER_CACHE_HPP___

#include "general.hpp"

/// Класс кэша бинарных шейдеров.
class ShaderCache : public Object
{
private:
	/// Файловая система для кэша шейдеров.
	ptr<FileSystem> fileSystem;
	/// Графическое устройство.
	ptr<Device> device;
	/// Компилятор шейдеров.
	ptr<DxShaderCompiler> shaderCompiler;

	/// Вычислятель хеша.
	ptr<Crypto::WhirlpoolStream> hashStream;

	/// Тип хэша.
	typedef unsigned char ShaderHash[64];
	/// Вычислить хэш шейдера.
	String CalculateHash(ptr<ShaderSource> shaderSource);

public:
	ShaderCache(ptr<FileSystem> fileSystem, ptr<Device> device, ptr<DxShaderCompiler> shaderCompiler);

	/// Получить бинарный шейдер.
	/** Если его нет в кэше, он компилируется и добавляется в него. */
	ptr<File> GetBinaryShader(ptr<ShaderSource> shaderSource);

	/// Получить вершинный шейдер.
	ptr<VertexShader> GetVertexShader(ptr<ShaderSource> shaderSource);
	/// Получить пиксельный шейдер.
	ptr<PixelShader> GetPixelShader(ptr<ShaderSource> shaderSource);
};

#endif
