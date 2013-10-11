#include "Material.hpp"

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

void Material::SetDiffuse(const vec4& diffuse)
{
	this->diffuse = diffuse;
}

void Material::SetSpecular(const vec4& specular)
{
	this->specular = specular;
}

void Material::SetNormalCoordTransform(const vec4& normalCoordTransform)
{
	this->normalCoordTransform = normalCoordTransform;
}

void Material::SetEnvironmentCoef(float environmentCoef)
{
	this->environmentCoef = environmentCoef;
}
