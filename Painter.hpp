#ifndef ___FARSH_PAINTER_HPP___
#define ___FARSH_PAINTER_HPP___

#include "general.hpp"
#include <unordered_map>

/// Класс, занимающийся рисованием моделей.
class Painter : public Object
{
private:
	ptr<Device> device;
	ptr<Context> context;
	ptr<Presenter> presenter;
	//** Размер экрана.
	int screenWidth, screenHeight;

public:
	/// Максимальное количество источников света без теней.
	static const int maxBasicLightsCount = 4;
	/// Максимальное количество источников света с тенями.
	static const int maxShadowLightsCount = 4;
	/// Количество для instancing'а.
	static const int maxInstancesCount = 64;

	//*** Атрибуты.
	Attribute<float4> aPosition;
	Attribute<float3> aNormal;
	Attribute<float2> aTexcoord;

	///*** Uniform-группа камеры.
	ptr<UniformGroup> ugCamera;
	/// Матрица вид-проекция.
	Uniform<float4x4> uViewProj;
	/// Положение камеры.
	Uniform<float3> uCameraPosition;

	/// Настройки семплера для карт теней.
	ptr<SamplerState> shadowSamplerState;

	/// Параметры простого источника света.
	struct BasicLight
	{
		/// Положение источника.
		Uniform<float3> uLightPosition;
		/// Цвет источника.
		Uniform<float3> uLightColor;

		BasicLight(ptr<UniformGroup> ug);
	};
	/// Параметры источника света с тенями.
	struct ShadowLight : public BasicLight
	{
		/// Матрица трансформации источника света.
		Uniform<float4x4> uLightTransform;
		/// Семплер карты теней источника света.
		Sampler<float, float2> uShadowSampler;

		ShadowLight(ptr<UniformGroup> ug, int samplerNumber);
	};
	/// Структура варианта света.
	struct LightVariant
	{
		/// Uniform-группа параметров.
		ptr<UniformGroup> ugLight;
		/// Uniform-буфер для параметров.
		ptr<UniformBuffer> ubLight;
		/// Рассеянный свет.
		Uniform<float3> uAmbientColor;
		/// Простые источники света.
		std::vector<BasicLight> basicLights;
		/// Источники света с тенями.
		std::vector<ShadowLight> shadowLights;
		/// Состояние контекста для opaque pass.
		ContextState csOpaque;

		LightVariant();
	};
	/// Варианты света.
	LightVariant lightVariants[maxBasicLightsCount + 1][maxShadowLightsCount + 1];

	///*** Uniform-группа материала.
	ptr<UniformGroup> ugMaterial;
	/// Семплер диффузной текстуры.
	Sampler<float3, float2> uDiffuseSampler;
	/// Семплер specular текстуры.
	Sampler<float3, float2> uSpecularSampler;

	///*** Uniform-группа модели.
	ptr<UniformGroup> ugModel;
	/// Матрицы мира.
	UniformArray<float4x4> uWorlds;

	//*** Uniform-буферы.
	ptr<UniformBuffer> ubShadow;
	ptr<UniformBuffer> ubCamera;
	ptr<UniformBuffer> ubMaterial;
	ptr<UniformBuffer> ubModel;

	//** Состояния конвейера.
	/// Состояние для shadow pass.
	ContextState shadowContextState;

	/// Размер карты теней.
	static const int shadowMapSize;

	//** Рендербуферы.
	ptr<RenderBuffer> rbBack;
	/// Буфер глубины.
	ptr<DepthStencilBuffer> dsbDepth;
	/// Карты теней.
	ptr<DepthStencilBuffer> dsbShadows[maxShadowLightsCount];

private:
	// Текущая камера для shadow pass.
	float4x4 shadowViewProj;
	// Текущая камера для opaque pass.
	float4x4 cameraViewProj;
	float3 cameraPosition;

	// Текущий вариант освещения.
	int basicLightsCount;
	int shadowLightsCount;

	/// Ключ шейдеров в кэше.
	struct ShaderKey
	{
		/// Количество источников света без теней.
		int basicLightsCount;
		/// Количество источников света с тенями.
		int shadowLightsCount;
		/// Skinned?
		bool skinned;

		ShaderKey(int basicLightsCount, int shadowLightsCount, bool skinned);

		/// Получить хеш.
		operator size_t() const;
	};
	/// Шейдер в кэше.
	struct Shader
	{
		ptr<VertexShader> vertexShader;
		ptr<PixelShader> pixelShader;
		Shader();
		Shader(ptr<VertexShader> vertexShader, ptr<PixelShader> pixelShader);
	};
	/// Кэш шейдеров.
	std::unordered_map<ShaderKey, Shader> shaders;

	/// Список моделей для рисования.
	struct Model
	{
		ptr<Texture> diffuseTexture;
		ptr<Texture> specularTexture;
		ptr<VertexBuffer> vertexBuffer;
		ptr<IndexBuffer> indexBuffer;
		float4x4 worldTransform;

		Model(ptr<Texture> diffuseTexture, ptr<Texture> specularTexture, ptr<VertexBuffer> vertexBuffer, ptr<IndexBuffer> indexBuffer, const float4x4& worldTransform);
	};
	std::vector<Model> models;

public:
	Painter(ptr<Device> device, ptr<Context> context, ptr<Presenter> presenter, int screenWidth, int screenHeight);

	/// Начать кадр.
	/** Очистить список моделей. */
	void BeginFrame();
	/// Зарегистрировать модель.
	void AddModel(ptr<Texture> diffuseTexture, ptr<Texture> specularTexture, ptr<VertexBuffer> vertexBuffer, ptr<IndexBuffer> indexBuffer, const float4x4& worldTransform);

	/// Выполнить shadow pass.
	void DoShadowPass(int shadowNumber, const float4x4& shadowViewProj);
	/// Начать opaque pass.
	void BeginOpaque(const float4x4& cameraViewProj, const float3& cameraPosition);
	/// Установить текущий вариант освещения.
	void SetLightVariant(int basicLightsCount, int shadowLightsCount);
	/// Установить рассеянный свет.
	void SetAmbientLight(const float3& ambientColor);
	/// Установить простой свет.
	void SetBasicLight(int basicLightNumber, const float3& lightPosition, const float3& lightColor);
	/// Установить свет с тенью.
	void SetShadowLight(int shadowLightNumber, const float3& lightPosition, const float3& lightColor, const float4x4& lightTransform);
	/// Применить параметры света.
	void ApplyLight();
	/// Нарисовать opaque pass.
	void DrawOpaque();
};

#endif
