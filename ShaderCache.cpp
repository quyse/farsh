#include "ShaderCache.hpp"

ShaderCache::ShaderCache(ptr<FileSystem> fileSystem, ptr<Device> device, ptr<DxShaderCompiler> shaderCompiler)
: fileSystem(fileSystem), device(device), shaderCompiler(shaderCompiler),
	hashStream(NEW(Crypto::WhirlpoolStream()))
{}

String ShaderCache::CalculateHash(ptr<ShaderSource> shaderSource)
{
	StreamWriter writer(hashStream);
	hashStream->Reset();

	writer.WriteString(shaderSource->GetFunctionName());
	writer.WriteString(shaderSource->GetProfile());
	hashStream->WriteFile(shaderSource->GetCode());
	hashStream->Flush();

	return hashStream->GetHashString();
}

ptr<File> ShaderCache::GetBinaryShader(ptr<ShaderSource> shaderSource)
{
	String hash = CalculateHash(shaderSource);

	// попробовать найти файл в кэше
	ptr<File> file = fileSystem->TryLoadFile(hash);
	if(file)
		return file;

	// скомпилировать
	file = shaderCompiler->Compile(shaderSource);
	// сохранить
	fileSystem->SaveFile(file, hash);
	// вернуть
	return file;
}

ptr<VertexShader> ShaderCache::GetVertexShader(ptr<ShaderSource> shaderSource)
{
	return device->CreateVertexShader(GetBinaryShader(shaderSource));
}

ptr<PixelShader> ShaderCache::GetPixelShader(ptr<ShaderSource> shaderSource)
{
	return device->CreatePixelShader(GetBinaryShader(shaderSource));
}
