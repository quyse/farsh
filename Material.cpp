#include "Material.hpp"

META_CLASS(Material, Farsh.Material);
	META_CONSTRUCTOR();
	META_METHOD(SetDiffuseTexture);
	META_METHOD(SetSpecularTexture);
	META_METHOD(SetNormalTexture);
	META_METHOD(SetDiffuse);
	META_METHOD(SetSpecular);
	META_METHOD(SetNormalCoordTransform);
	META_METHOD(SetEnvironmentCoef);
META_CLASS_END();

//*** MaterialKey

MaterialKey::MaterialKey(bool hasDiffuseTexture, bool hasSpecularTexture, bool hasNormalTexture, bool useEnvironment) :
	hasDiffuseTexture(hasDiffuseTexture), hasSpecularTexture(hasSpecularTexture), hasNormalTexture(hasNormalTexture),
	useEnvironment(useEnvironment) {}

bool operator==(const MaterialKey& a, const MaterialKey& b)
{
	return
		a.hasDiffuseTexture == b.hasDiffuseTexture &&
		a.hasSpecularTexture == b.hasSpecularTexture &&
		a.hasNormalTexture == b.hasNormalTexture &&
		a.useEnvironment == b.useEnvironment;
}

//*** Material

Material::Material()
: diffuse(1, 1, 1, 1), specular(1, 1, 1, 1), normalCoordTransform(1, 1, 0, 0) {}

MaterialKey Material::GetKey() const
{
	return MaterialKey(diffuseTexture, specularTexture, normalTexture, environmentCoef > 0);
}

void Material::SetDiffuseTexture(ptr<Texture> diffuseTexture)
{
	this->diffuseTexture = diffuseTexture;
}

void Material::SetSpecularTexture(ptr<Texture> specularTexture)
{
	this->specularTexture = specularTexture;
}

void Material::SetNormalTexture(ptr<Texture> normalTexture)
{
	this->normalTexture = normalTexture;
}

void Material::SetDiffuse(float red, float green, float blue, float alpha)
{
	this->diffuse = vec4(red, green, blue, alpha);
}

void Material::SetSpecular(float red, float green, float blue, float glossiness)
{
	this->specular = vec4(red, green, blue, glossiness);
}

void Material::SetNormalCoordTransform(float scaleX, float scaleY, float offsetX, float offsetY)
{
	this->normalCoordTransform = vec4(scaleX, scaleY, offsetX, offsetY);
}

void Material::SetEnvironmentCoef(float environmentCoef)
{
	this->environmentCoef = environmentCoef;
}
