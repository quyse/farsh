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
	/// Случайная текстура.
	ptr<Texture> randomTexture;

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
	/// Семплер случайной текстуры.
	Sampler<float4, float2> uRandomSampler;
	/// Семплер диффузной текстуры.
	Sampler<float3, float2> uDiffuseSampler;
	/// Семплер specular текстуры.
	Sampler<float3, float2> uSpecularSampler;

	///*** Uniform-группа модели.
	ptr<UniformGroup> ugModel;
	/// Матрицы мира.
	UniformArray<float4x4> uWorlds;

	///*** Uniform-группа даунсемплинга.
	ptr<UniformGroup> ugDownsample;
	/// Смещения для семплов.
	/** Смещения это xz, xw, yz, yw. */
	Uniform<float4> uDownsampleOffsets;
	/// Исходный семплер.
	Sampler<float3, float2> uDownsampleSourceSampler;
	/// Исходный семплер для освещённости.
	Sampler<float, float2> uDownsampleLuminanceSourceSampler;

	///*** Uniform-группа bloom.
	ptr<UniformGroup> ugBloom;
	/// Ограничение по освещённости для bloom.
	Uniform<float> uBloomLimit;
	/// Семплер исходника для bloom.
	Sampler<float3, float2> uBloomSourceSampler;

	///*** Uniform-группа tone mapping.
	ptr<UniformGroup> ugTone;
	/// Коэффициент для получения относительной освещённости.
	Uniform<float> uToneLuminanceKey;
	/// Максимальная освещённость.
	Uniform<float> uToneMaxLuminance;
	/// Семплер результата bloom.
	Sampler<float3, float2> uToneBloomSampler;
	/// Семплер экрана.
	Sampler<float3, float2> uToneScreenSampler;
	/// Семплер результата downsample для средней освещённости.
	Sampler<float, float2> uToneAverageSampler;

	//*** Uniform-буферы.
	ptr<UniformBuffer> ubCamera;
	ptr<UniformBuffer> ubMaterial;
	ptr<UniformBuffer> ubModel;
	ptr<UniformBuffer> ubDownsample;
	ptr<UniformBuffer> ubBloom;
	ptr<UniformBuffer> ubTone;

	//** Состояния конвейера.
	/// Состояние для shadow pass.
	ContextState shadowContextState;

	/// Размер карты теней.
	static const int shadowMapSize;
	/// Размер случайной текстуры.
	static const int randomMapSize;
	/// Количество проходов downsampling.
	static const int downsamplingPassesCount = 10;
	/// Номер прохода, после которого делать bloom.
	static const int downsamplingStepForBloom;
	/// Размер карты для bloom.
	static const int bloomMapSize;

	//** Постпроцессинг.
	/// Состояния для downsampling.
	ContextState csDownsamples[downsamplingPassesCount];
	/// Самый первый проход bloom.
	ContextState csBloomLimit;
	/// Первый проход bloom.
	ContextState csBloom1;
	/// Второй проход bloom.
	ContextState csBloom2;
	/// Tone mapping.
	ContextState csTone;

	//** Рендербуферы.
	/// HDR-текстура для изначального рисования.
	ptr<RenderBuffer> rbScreen;
	/// HDR-буферы для downsampling.
	ptr<RenderBuffer> rbDownsamples[downsamplingPassesCount];
	/// HDR-буферы для Bloom.
	ptr<RenderBuffer> rbBloom1, rbBloom2;
	/// Backbuffer.
	ptr<RenderBuffer> rbBack;
	/// Буфер глубины.
	ptr<DepthStencilBuffer> dsbDepth;
	/// Карты теней.
	ptr<DepthStencilBuffer> dsbShadows[maxShadowLightsCount];

private:
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

	//*** зарегистрированные объекты для рисования

	// Текущая камера для opaque pass.
	float4x4 cameraViewProj;
	float3 cameraPosition;

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

	// Источники света.
	/// Рассеянный свет.
	float3 ambientColor;
	/// Структура источника света.
	struct Light
	{
		float3 position;
		float3 color;
		float4x4 transform;
		bool shadow;

		Light(const float3& position, const float3& color);
		Light(const float3& position, const float3& color, const float4x4& transform);
	};
	std::vector<Light> lights;

public:
	Painter(ptr<Device> device, ptr<Context> context, ptr<Presenter> presenter, int screenWidth, int screenHeight);

	/// Начать кадр.
	/** Очистить все регистрационные списки. */
	void BeginFrame();
	/// Установить камеру.
	void SetCamera(const float4x4& cameraViewProj, const float3& cameraPosition);
	/// Зарегистрировать модель.
	void AddModel(ptr<Texture> diffuseTexture, ptr<Texture> specularTexture, ptr<VertexBuffer> vertexBuffer, ptr<IndexBuffer> indexBuffer, const float4x4& worldTransform);
	/// Установить рассеянный свет.
	void SetAmbientColor(const float3& ambientColor);
	/// Зарегистрировать простой источник света.
	void AddBasicLight(const float3& position, const float3& color);
	/// Зарегистрировать источник света с тенью.
	void AddShadowLight(const float3& position, const float3& color, const float4x4& transform);

	/// Выполнить рисование.
	void Draw();
};

#endif
