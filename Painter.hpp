#ifndef ___FARSH_PAINTER_HPP___
#define ___FARSH_PAINTER_HPP___

#include "general.hpp"
#include "Geometry.hpp"
#include "Material.hpp"
#include <unordered_map>

class BoneAnimationFrame;
class GeometryFormats;

/// Класс, занимающийся рисованием моделей.
class Painter : public Object
{
private:
	/// Параметры простого источника света.
	struct BasicLight
	{
		/// Положение источника.
		Uniform<vec3> uLightPosition;
		/// Цвет источника.
		Uniform<vec3> uLightColor;

		BasicLight(ptr<UniformGroup> ug);
	};
	/// Параметры источника света с тенями.
	struct ShadowLight : public BasicLight
	{
		/// Матрица трансформации источника света.
		Uniform<mat4x4> uLightTransform;
		/// Семплер карты теней источника света.
		Sampler<float, 2> uShadowSampler;

		ShadowLight(ptr<UniformGroup> ug, int samplerNumber);
	};

	/// Структура ключа варианта света.
	struct LightVariantKey
	{
		int basicLightsCount;
		int shadowLightsCount;

		LightVariantKey(int basicLightsCount, int shadowLightsCount)
		: basicLightsCount(basicLightsCount), shadowLightsCount(shadowLightsCount)
		{}
	};
	/// Структура варианта света.
	struct LightVariant
	{
		/// Uniform-группа параметров.
		ptr<UniformGroup> ugLight;
		/// Рассеянный свет.
		Uniform<vec3> uAmbientColor;
		/// Простые источники света.
		std::vector<BasicLight> basicLights;
		/// Источники света с тенями.
		std::vector<ShadowLight> shadowLights;

		LightVariant();
	};

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
	};

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
	};

	struct Hasher
	{
		size_t operator()(const LightVariantKey& key) const;
		size_t operator()(const VertexShaderKey& key) const;
		size_t operator()(const PixelShaderKey& key) const;
		size_t operator()(const MaterialKey& key) const;
	};

	friend bool operator==(const LightVariantKey& a, const LightVariantKey& b);
	friend bool operator==(const VertexShaderKey& a, const VertexShaderKey& b);
	friend bool operator==(const PixelShaderKey& a, const PixelShaderKey& b);

private:
	ptr<Device> device;
	ptr<Context> context;
	ptr<Presenter> presenter;
	//** Размер экрана.
	int screenWidth, screenHeight;
	/// Кэш бинарных шейдеров.
	ptr<ShaderCache> shaderCache;
	/// Форматы геометрии.
	ptr<GeometryFormats> geometryFormats;

	/// Текстура окружения.
	ptr<Texture> environmentTexture;

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
	ptr<AttributeBinding> ab;
	ptr<Instancer> instancer;
	ptr<AttributeBinding> abInstanced;
	Value<vec3> aPosition;
	Value<vec3> aNormal;
	Value<vec2> aTexcoord;
	ptr<AttributeBinding> abSkinned;
	Value<vec3> aSkinnedPosition;
	Value<vec3> aSkinnedNormal;
	Value<vec2> aSkinnedTexcoord;
	Value<uvec4> aSkinnedBoneNumbers;
	Value<vec4> aSkinnedBoneWeights;

	///*** Uniform-группа камеры.
	ptr<UniformGroup> ugCamera;
	/// Матрица вид-проекция.
	Uniform<mat4x4> uViewProj;
	/// Обратная матрица вид-проекция.
	Uniform<mat4x4> uInvViewProj;
	/// Положение камеры.
	Uniform<vec3> uCameraPosition;

	/// Настройки семплера для карт теней.
	ptr<SamplerState> shadowSamplerState;

	/// Варианты света.
	std::unordered_map<LightVariantKey, LightVariant, Hasher> lightVariantsCache;
	/// Получить вариант света.
	LightVariant& GetLightVariant(const LightVariantKey& key);

	///*** Uniform-группа материала.
	ptr<UniformGroup> ugMaterial;
	/// Диффузный цвет с альфа-каналом.
	Uniform<vec4> uDiffuse;
	/// Specular + glossiness.
	Uniform<vec4> uSpecular;
	/// Преобразование текстурных координат для карты нормалей.
	Uniform<vec4> uNormalCoordTransform;
	/// Семплер диффузной текстуры.
	Sampler<vec4, 2> uDiffuseSampler;
	/// Семплер specular текстуры.
	Sampler<vec4, 2> uSpecularSampler;
	/// Семплер карты нормалей.
	Sampler<vec3, 2> uNormalSampler;
	/// Семплер текстуры окружения.
	SamplerCube<vec3> uEnvironmentSampler;

	///*** Uniform-группа модели.
	ptr<UniformGroup> ugModel;
	/// Матрица мира.
	Uniform<mat4x4> uWorld;

	///*** Uniform-группа instanced-модели.
	ptr<UniformGroup> ugInstancedModel;
	/// Матрицы мира.
	UniformArray<mat4x4> uWorlds;

	///*** Uniform-группа skinned-модели.
	ptr<UniformGroup> ugSkinnedModel;
	/// Кватернионы костей.
	UniformArray<vec4> uBoneOrientations;
	/// Смещения костей.
	UniformArray<vec4> uBoneOffsets;

	///*** Uniform-группа декалей.
	ptr<UniformGroup> ugDecal;
	/// Матрицы декалей.
	UniformArray<mat4x4> uDecalTransforms;
	/// Обратные матрицы декалей.
	UniformArray<mat4x4> uDecalInvTransforms;
	/// Семплер нормалей.
	Sampler<vec3, 2> uScreenNormalSampler;
	/// Семплер глубины.
	Sampler<float, 2> uScreenDepthSampler;

	///*** Uniform-группа размытия тени.
	ptr<UniformGroup> ugShadowBlur;
	/// Вектор направления размытия.
	Uniform<vec2> uShadowBlurDirection;
	/// Семплер для тени.
	Sampler<float, 2> uShadowBlurSourceSampler;

	///*** Uniform-группа даунсемплинга.
	ptr<UniformGroup> ugDownsample;
	/// Смещения для семплов.
	/** Смещения это xz, xw, yz, yw. */
	Uniform<vec4> uDownsampleOffsets;
	/// Коэффициент смешивания.
	Uniform<float> uDownsampleBlend;
	/// Исходный семплер.
	Sampler<vec3, 2> uDownsampleSourceSampler;
	/// Исходный семплер для освещённости.
	Sampler<float, 2> uDownsampleLuminanceSourceSampler;

	///*** Uniform-группа bloom.
	ptr<UniformGroup> ugBloom;
	/// Ограничение по освещённости для bloom.
	Uniform<float> uBloomLimit;
	/// Семплер исходника для bloom.
	Sampler<vec3, 2> uBloomSourceSampler;

	///*** Uniform-группа tone mapping.
	ptr<UniformGroup> ugTone;
	/// Коэффициент для получения относительной освещённости.
	Uniform<float> uToneLuminanceKey;
	/// Максимальная освещённость.
	Uniform<float> uToneMaxLuminance;
	/// Семплер результата bloom.
	Sampler<vec3, 2> uToneBloomSampler;
	/// Семплер экрана.
	Sampler<vec3, 2> uToneScreenSampler;
	/// Семплер результата downsample для средней освещённости.
	Sampler<float, 2> uToneAverageSampler;

	//*** Промежуточные переменные.
	Interpolant<vec3> iNormal;
	Interpolant<vec2> iTexcoord;
	Interpolant<vec3> iWorldPosition;
	Interpolant<float> iDepth;
	Interpolant<vec4> iScreen;
	Interpolant<uint> iInstance;

	//*** Декали.
	struct DecalStuff
	{
		struct Vertex
		{
			vec4 position;
			vec3 normal;
			vec2 texcoord;
		};

		ptr<VertexLayout> vl;
		ptr<AttributeLayout> al;
		ptr<AttributeLayoutSlot> als;
		Value<vec4> aPosition;
		Value<vec3> aNormal;
		Value<vec2> aTexcoord;

		ptr<VertexBuffer> vb;
		ptr<IndexBuffer> ib;

		ptr<AttributeBinding> ab;

		ptr<BlendState> bs;

		DecalStuff(ptr<Device> device);
	};
	DecalStuff decalStuff;

	//***
	ptr<AttributeBinding> abFilter;
	ptr<VertexBuffer> vbFilter;
	ptr<IndexBuffer> ibFilter;
	ptr<VertexShader> vsFilter;
	ptr<PixelShader> psShadowBlur;
	ptr<PixelShader> psDownsample;
	ptr<PixelShader> psDownsampleLuminanceFirst;
	ptr<PixelShader> psDownsampleLuminance;
	ptr<PixelShader> psBloomLimit, psBloom1, psBloom2, psTone;

	ptr<SamplerState> ssPoint;
	ptr<SamplerState> ssLinear;
	ptr<SamplerState> ssPointBorder;
	ptr<SamplerState> ssColorTexture;

	ptr<BlendState> bsLastDownsample;

	ptr<PixelShader> psShadow;

	/// Размер карты теней.
	static const int shadowMapSize;
	/// Количество проходов downsampling.
	static const int downsamplingPassesCount = 10;
	/// Номер прохода, после которого делать bloom.
	static const int downsamplingStepForBloom;
	/// Размер карты для bloom.
	static const int bloomMapSize;

	//** Рендербуферы.
	/// HDR-текстура для изначального рисования.
	ptr<RenderBuffer> rbScreen;
	/// Экранная карта нормалей.
	ptr<RenderBuffer> rbScreenNormal;
	/// Фреймбуферы для downsampling.
	ptr<FrameBuffer> fbDownsamples[downsamplingPassesCount];
	/// Фреймбуферы для bloom.
	ptr<FrameBuffer> fbBloom1, fbBloom2;
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
	/// Фреймбуферы для карт теней.
	ptr<FrameBuffer> fbShadows[maxShadowLightsCount];
	/// Фреймбуферы для размытия карт теней.
	ptr<FrameBuffer> fbShadowBlurs[maxShadowLightsCount];
	/// Фреймбуфер для первого шага размытия карт теней.
	ptr<FrameBuffer> fbShadowBlur1;
	/// Вспомогательная карта для размытия.
	ptr<RenderBuffer> rbShadowBlur;
	/// Основной фреймбуфер.
	ptr<FrameBuffer> fbOpaque;
	/// Фреймбуфер для декалей.
	ptr<FrameBuffer> fbDecal;

private:
	/// Кэш вершинных шейдеров.
	std::unordered_map<VertexShaderKey, ptr<VertexShader>, Hasher> vertexShaderCache;
	/// Получить вершинный шейдер.
	ptr<VertexShader> GetVertexShader(const VertexShaderKey& key);
	/// Кэш вершинных шейдеров для теневого прохода.
	std::unordered_map<VertexShaderKey, ptr<VertexShader>, Hasher> vertexShadowShaderCache;
	/// Получить вершинный шейдер для теневого прохода.
	ptr<VertexShader> GetVertexShadowShader(const VertexShaderKey& key);

	/// Временные переменные вершинного шейдера моделей.
	Temp<vec4> tmpVertexPosition;
	Temp<vec3> tmpVertexNormal;

	/// Повернуть вектор кватернионом.
	static Value<vec3> ApplyQuaternion(Value<vec4> q, Value<vec3> v);
	/// Получить положение вершины и нормаль в мире.
	/** Возвращает выражение, которое записывает положение и нормаль во
	временные переменные tmpVertexPosition и tmpVertexNormal. */
	Expression GetWorldPositionAndNormal(const VertexShaderKey& key);

	/// Кэш пиксельных шейдеров.
	std::unordered_map<PixelShaderKey, ptr<PixelShader>, Hasher> pixelShaderCache;
	/// Получить пиксельный шейдер.
	ptr<PixelShader> GetPixelShader(const PixelShaderKey& key);

	//*** Временные переменные пиксельного шейдера материала.
	Temp<vec4> tmpWorldPosition;
	Temp<vec2> tmpTexcoord;
	Temp<vec3> tmpNormal;
	Temp<vec3> tmpToCamera;
	Temp<vec4> tmpDiffuse, tmpSpecular;
	Temp<float> tmpSpecularExponent;
	Temp<vec3> tmpColor;

	/// Получить временные переменные для освещения в пиксельном шейдере.
	Expression BeginMaterialLighting(const PixelShaderKey& key, Value<vec3> ambientColor);
	/// Вычислить добавку к цвету и прибавить её к tmpColor.
	Expression ApplyMaterialLighting(Value<vec3> lightPosition, Value<vec3> lightColor);

	/// Текущее время кадра.
	float frameTime;

	//*** зарегистрированные объекты для рисования

	// Текущая камера для opaque pass.
	mat4x4 cameraViewProj;
	mat4x4 cameraInvViewProj;
	vec3 cameraPosition;

	/// Модель для рисования.
	struct Model
	{
		ptr<Material> material;
		ptr<Geometry> geometry;
		mat4x4 worldTransform;

		Model(ptr<Material> material, ptr<Geometry> geometry, const mat4x4& worldTransform);
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
		mat4x4 transform;
		mat4x4 invTransform;

		Decal(ptr<Material> material, const mat4x4& transform, const mat4x4& invTransform);
	};
	std::vector<Decal> decals;

	// Источники света.
	/// Рассеянный свет.
	vec3 ambientColor;
	/// Структура источника света.
	struct Light
	{
		vec3 position;
		vec3 color;
		mat4x4 transform;
		bool shadow;

		Light(const vec3& position, const vec3& color);
		Light(const vec3& position, const vec3& color, const mat4x4& transform);
	};
	std::vector<Light> lights;

	// Параметры постпроцессинга.
	float bloomLimit, toneLuminanceKey, toneMaxLuminance;

	/// Сгенерировать вершинный шейдер.
	ptr<VertexShader> GenerateVS(Expression expression);
	/// Сгенерировать пиксельный шейдер.
	ptr<PixelShader> GeneratePS(Expression expression);

public:
	Painter(ptr<Device> device, ptr<Context> context, ptr<Presenter> presenter, int screenWidth, int screenHeight, ptr<ShaderCache> shaderCache, ptr<GeometryFormats> geometryFormats);

	/// Начать кадр.
	/** Очистить все регистрационные списки. */
	void BeginFrame(float frameTime);
	/// Установить камеру.
	void SetCamera(const mat4x4& cameraViewProj, const vec3& cameraPosition);
	/// Зарегистрировать модель.
	void AddModel(ptr<Material> material, ptr<Geometry> geometry, const mat4x4& worldTransform);
	/// Зарегистрировать skinned-модель.
	void AddSkinnedModel(ptr<Material> material, ptr<Geometry> geometry, ptr<BoneAnimationFrame> animationFrame);
	void AddSkinnedModel(ptr<Material> material, ptr<Geometry> geometry, ptr<Geometry> shadowGeometry, ptr<BoneAnimationFrame> animationFrame);
	/// Добавить декаль.
	void AddDecal(ptr<Material> material, const mat4x4& transform, const mat4x4& invTransform);
	/// Установить рассеянный свет.
	void SetAmbientColor(const vec3& ambientColor);
	/// Установить текстуру окружения.
	void SetEnvironmentTexture(ptr<Texture> environmentTexture);
	/// Зарегистрировать простой источник света.
	void AddBasicLight(const vec3& position, const vec3& color);
	/// Зарегистрировать источник света с тенью.
	void AddShadowLight(const vec3& position, const vec3& color, const mat4x4& transform);

	/// Установить параметры постпроцессинга.
	void SetupPostprocess(float bloomLimit, float toneLuminanceKey, float toneMaxLuminance);

	/// Выполнить рисование.
	void Draw();
};

#endif
