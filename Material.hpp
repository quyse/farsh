#ifndef ___FARSH_MATERIAL_HPP___
#define ___FARSH_MATERIAL_HPP___

#include "general.hpp"

/// Структура ключа материала.
struct MaterialKey
{
	bool hasDiffuseTexture;
	bool hasSpecularTexture;
	bool hasNormalTexture;

	MaterialKey(bool hasDiffuseTexture, bool hasSpecularTexture, bool hasNormalTexture);
	operator size_t() const;
};

/// Структура материала.
struct Material : public Object
{
	ptr<Texture> diffuseTexture;
	ptr<Texture> specularTexture;
	ptr<Texture> normalTexture;
	vec4 diffuse;
	vec4 specular;
	vec4 normalCoordTransform;

	Material();

	MaterialKey GetKey() const;

	//******* Методы для скрипта.
	void SetDiffuseTexture(ptr<Texture> diffuseTexture);
	void SetSpecularTexture(ptr<Texture> specularTexture);
	void SetNormalTexture(ptr<Texture> normalTexture);
	void SetDiffuse(float red, float green, float blue, float alpha);
	void SetSpecular(float red, float green, float blue, float glossiness);
	void SetNormalCoordTransform(float scaleX, float scaleY, float offsetX, float offsetY);

	SCRIPTABLE_CLASS(Material);
};

#endif
