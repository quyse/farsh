#ifndef ___FARSH_PAINTER_HPP___
#define ___FARSH_PAINTER_HPP___

#include "general.hpp"
#include "Material.hpp"
#include <unordered_map>

class BoneAnimationFrame;

/// Класс, занимающийся рисованием моделей.
class Painter : public Object
{
private:
	ptr<Device> device;
	ptr<Context> context;
	ptr<Presenter> presenter;
	//** Размер экрана.
	int screenWidth, screenHeight;
	/// Кэш бинарных шейдеров.
	ptr<ShaderCache> shaderCache;

	/// Случайная текстура.
	ptr<Texture> randomTexture;

	/// Геометрия декалей.
	ptr<Geometry> geometryDecal;

	/// Состояние смешивания для декалей.
	ptr<BlendState> bsDecal;

	/// Максимальное количество источников света без теней.
	static const int maxBasicLightsCount = 4;
	/// Максимальное количество источников света с тенями.
	static const int maxShadowLightsCount = 4;
	/// Количество для instancing'а.
	static const int maxInstancesCount = 64;
	/// Количество костей для skinning.
	static const int maxBonesCount = 64;
	/// Количество декалей.
	static const int maxDecalsCount = 64;

	//*** Атрибуты.
	Attribute<float4> aPosition;
	Attribute<float3> aNormal;
	Attribute<float2> aTexcoord;
	Attribute<uint4> aBoneNumbers;
	Attribute<float4> aBoneWeights;

	///*** Uniform-группа камеры.
	ptr<UniformGroup> ugCamera;
	/// Матрица вид-проекция.
	Uniform<float4x4> uViewProj;
	/// Обратная матрица вид-проекция.
	Uniform<float4x4> uInvViewProj;
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
	/// Структура ключа варианта света.
	struct LightVariantKey
	{
		int basicLightsCount;
		int shadowLightsCount;

		LightVariantKey(int basicLightsCount, int shadowLightsCount)
		: basicLightsCount(basicLightsCount), shadowLightsCount(shadowLightsCount)
		{}

		operator size_t() const
		{
			return basicLightsCount | (shadowLightsCount << 3);
		}
	};
	/// Варианты света.
	std::unordered_map<LightVariantKey, LightVariant> lightVariantsCache;
	/// Получить вариант света.
	LightVariant& GetLightVariant(const LightVariantKey& key);

	///*** Uniform-группа материала.
	ptr<UniformGroup> ugMaterial;
	/// Диффузный цвет с альфа-каналом.
	Uniform<float4> uDiffuse;
	/// Specular + glossiness.
	Uniform<float4> uSpecular;
	/// Преобразование текстурных координат для карты нормалей.
	Uniform<float4> uNormalCoordTransform;
	/// Семплер диффузной текстуры.
	Sampler<float4, float2> uDiffuseSampler;
	/// Семплер specular текстуры.
	Sampler<float4, float2> uSpecularSampler;
	/// Семплер карты нормалей.
	Sampler<float3, float2> uNormalSampler;

	///*** Uniform-группа модели.
	ptr<UniformGroup> ugModel;
	/// Матрица мира.
	Uniform<float4x4> uWorld;

	///*** Uniform-группа instanced-модели.
	ptr<UniformGroup> ugInstancedModel;
	/// Матрицы мира.
	UniformArray<float4x4> uWorlds;

	///*** Uniform-группа skinned-модели.
	ptr<UniformGroup> ugSkinnedModel;
	/// Кватернионы костей.
	UniformArray<float4> uBoneOrientations;
	/// Смещения костей.
	UniformArray<float4> uBoneOffsets;

	///*** Uniform-группа декалей.
	ptr<UniformGroup> ugDecal;
	/// Матрицы декалей.
	UniformArray<float4x4> uDecalTransforms;
	/// Обратные матрицы декалей.
	UniformArray<float4x4> uDecalInvTransforms;
	/// Семплер нормалей.
	Sampler<float3, float2> uScreenNormalSampler;
	/// Семплер глубины.
	Sampler<float, float2> uScreenDepthSampler;

	///*** Uniform-группа размытия тени.
	ptr<UniformGroup> ugShadowBlur;
	/// Вектор направления размытия.
	Uniform<float2> uShadowBlurDirection;
	/// Семплер для тени.
	Sampler<float, float2> uShadowBlurSourceSampler;

	///*** Uniform-группа даунсемплинга.
	ptr<UniformGroup> ugDownsample;
	/// Смещения для семплов.
	/** Смещения это xz, xw, yz, yw. */
	Uniform<float4> uDownsampleOffsets;
	/// Коэффициент смешивания.
	Uniform<float> uDownsampleBlend;
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

	//*** Промежуточные переменные.
	Interpolant<float3> iNormal;
	Interpolant<float2> iTexcoord;
	Interpolant<float3> iWorldPosition;
	Interpolant<float> iDepth;
	Interpolant<float4> iScreen;
	Interpolant<uint> iInstance;

	//*** Выходные переменные.
	Fragment<float4> fTarget;
	Fragment<float4> fNormal;

	//** Состояния конвейера.
	/// Состояние для shadow pass.
	ContextState csShadow;
	/// Состояние для прохода размытия.
	ContextState csShadowBlur;

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
	/// Экранная карта нормалей.
	ptr<RenderBuffer> rbScreenNormal;
	/// HDR-буферы для downsampling.
	ptr<RenderBuffer> rbDownsamples[downsamplingPassesCount];
	/// HDR-буферы для Bloom.
	ptr<RenderBuffer> rbBloom1, rbBloom2;
	/// Backbuffer.
	ptr<RenderBuffer> rbBack;
	/// Буфер глубины.
	ptr<DepthStencilBuffer> dsbDepth;
	/// Буфер глубины для карт теней.
	ptr<DepthStencilBuffer> dsbShadow;
	/// Карты теней.
	ptr<RenderBuffer> rbShadows[maxShadowLightsCount];
	/// Вспомогательная карта для размытия.
	ptr<RenderBuffer> rbShadowBlur;

private:
	/// Ключ вершинного шейдера в кэше.
	struct VertexShaderKey
	{
		/// Instanced?
		bool instanced;
		/// Скиннинг?
		/** Только при instanced=false. */
		bool skinned;
		/// Декаль?
		/** Только при instanced = true, skinned = false. */
		bool decal;

		VertexShaderKey(bool instanced, bool skinned, bool decal);

		operator size_t() const;
	};
	/// Кэш вершинных шейдеров.
	std::unordered_map<VertexShaderKey, ptr<VertexShader> > vertexShaderCache;
	/// Получить вершинный шейдер.
	ptr<VertexShader> GetVertexShader(const VertexShaderKey& key);
	/// Кэш вершинных шейдеров для теневого прохода.
	std::unordered_map<VertexShaderKey, ptr<VertexShader> > vertexShadowShaderCache;
	/// Получить вершинный шейдер для теневого прохода.
	ptr<VertexShader> GetVertexShadowShader(const VertexShaderKey& key);

	/// Временные переменные вершинного шейдера моделей.
	Temp<float4> tmpVertexPosition;
	Temp<float3> tmpVertexNormal;

	/// Повернуть вектор кватернионом.
	static Value<float3> ApplyQuaternion(Value<float4> q, Value<float3> v);
	/// Получить положение вершины и нормаль в мире.
	/** Возвращает выражение, которое записывает положение и нормаль во
	временные переменные tmpVertexPosition и tmpVertexNormal. */
	Expression GetWorldPositionAndNormal(const VertexShaderKey& key);

	/// Ключ пиксельного шейдера в кэше.
	struct PixelShaderKey
	{
		/// Количество источников света без теней.
		int basicLightsCount;
		/// Количество источников света с тенями.
		int shadowLightsCount;
		/// Декаль?
		bool decal;
		/// Ключ материала.
		MaterialKey materialKey;

		PixelShaderKey(int basicLightsCount, int shadowLightsCount, bool decal, const MaterialKey& materialKey);

		/// Получить хеш.
		operator size_t() const;
	};
	/// Кэш пиксельных шейдеров.
	std::unordered_map<PixelShaderKey, ptr<PixelShader> > pixelShaderCache;
	/// Получить пиксельный шейдер.
	ptr<PixelShader> GetPixelShader(const PixelShaderKey& key);

	//*** Временные переменные пиксельного шейдера материала.
	Temp<float4> tmpWorldPosition;
	Temp<float2> tmpTexcoord;
	Temp<float3> tmpNormal;
	Temp<float3> tmpToCamera;
	Temp<float4> tmpDiffuse, tmpSpecular;
	Temp<float> tmpSpecularExponent;
	Temp<float3> tmpColor;

	/// Получить временные переменные для освещения в пиксельном шейдере.
	Expression BeginMaterialLighting(const PixelShaderKey& key, Value<float3> ambientColor);
	/// Вычислить добавку к цвету и прибавить её к tmpColor.
	Expression ApplyMaterialLighting(Value<float3> lightPosition, Value<float3> lightColor);

	/// Текущее время кадра.
	float frameTime;

	//*** зарегистрированные объекты для рисования

	// Текущая камера для opaque pass.
	float4x4 cameraViewProj;
	float4x4 cameraInvViewProj;
	float3 cameraPosition;

	/// Модель для рисования.
	struct Model
	{
		ptr<Material> material;
		ptr<Geometry> geometry;
		float4x4 worldTransform;

		Model(ptr<Material> material, ptr<Geometry> geometry, const float4x4& worldTransform);
	};
	std::vector<Model> models;

	/// Skinned модель для рисования.
	struct SkinnedModel
	{
		ptr<Material> material;
		ptr<Geometry> geometry;
		ptr<Geometry> shadowGeometry;
		/// Настроенный кадр анимации.
		ptr<BoneAnimationFrame> animationFrame;

		SkinnedModel(ptr<Material> material, ptr<Geometry> geometry, ptr<Geometry> shadowGeometry, ptr<BoneAnimationFrame> animationFrame);
	};
	std::vector<SkinnedModel> skinnedModels;

	/// Декаль для рисования.
	struct Decal
	{
		ptr<Material> material;
		float4x4 transform;
		float4x4 invTransform;

		Decal(ptr<Material> material, const float4x4& transform, const float4x4& invTransform);
	};
	std::vector<Decal> decals;

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

	// Параметры постпроцессинга.
	float bloomLimit, toneLuminanceKey, toneMaxLuminance;

	/// Сгенерировать вершинный шейдер.
	ptr<VertexShader> GenerateVS(Expression expression);
	/// Сгенерировать пиксельный шейдер.
	ptr<PixelShader> GeneratePS(Expression expression);

public:
	Painter(ptr<Device> device, ptr<Context> context, ptr<Presenter> presenter, int screenWidth, int screenHeight, ptr<ShaderCache> shaderCache);

	/// Начать кадр.
	/** Очистить все регистрационные списки. */
	void BeginFrame(float frameTime);
	/// Установить камеру.
	void SetCamera(const float4x4& cameraViewProj, const float3& cameraPosition);
	/// Зарегистрировать модель.
	void AddModel(ptr<Material> material, ptr<Geometry> geometry, const float4x4& worldTransform);
	/// Зарегистрировать skinned-модель.
	void AddSkinnedModel(ptr<Material> material, ptr<Geometry> geometry, ptr<BoneAnimationFrame> animationFrame);
	void AddSkinnedModel(ptr<Material> material, ptr<Geometry> geometry, ptr<Geometry> shadowGeometry, ptr<BoneAnimationFrame> animationFrame);
	/// Добавить декаль.
	void AddDecal(ptr<Material> material, const float4x4& transform, const float4x4& invTransform);
	/// Установить рассеянный свет.
	void SetAmbientColor(const float3& ambientColor);
	/// Зарегистрировать простой источник света.
	void AddBasicLight(const float3& position, const float3& color);
	/// Зарегистрировать источник света с тенью.
	void AddShadowLight(const float3& position, const float3& color, const float4x4& transform);

	/// Установить параметры постпроцессинга.
	void SetupPostprocess(float bloomLimit, float toneLuminanceKey, float toneMaxLuminance);

	/// Выполнить рисование.
	void Draw();
};

#endif
