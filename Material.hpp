#ifndef ___FARSH_MATERIAL_HPP___
#define ___FARSH_MATERIAL_HPP___

#include "general.hpp"

/// Структура ключа материала.
struct MaterialKey
{
	bool hasDiffuseTexture;
	bool hasSpecularTexture;
	bool hasNormalTexture;
	bool useEnvironment;

	MaterialKey(bool hasDiffuseTexture, bool hasSpecularTexture, bool hasNormalTexture, bool useEnvironment);
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
	/// Коэффициент примешивания окружения к цвету.
	float environmentCoef;

	Material();

	MaterialKey GetKey() const;

	//******* Методы для скрипта.
	void SetDiffuseTexture(ptr<Texture> diffuseTexture);
	void SetSpecularTexture(ptr<Texture> specularTexture);
	void SetNormalTexture(ptr<Texture> normalTexture);
	void SetDiffuse(float red, float green, float blue, float alpha);
	void SetSpecular(float red, float green, float blue, float glossiness);
	void SetNormalCoordTransform(float scaleX, float scaleY, float offsetX, float offsetY);
	void SetEnvironmentCoef(float environmentCoef);

	META_DECLARE_CLASS(Material);
};

#endif
