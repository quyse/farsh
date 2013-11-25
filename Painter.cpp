#include "Painter.hpp"
#include "BoneAnimation.hpp"
#include "GeometryFormats.hpp"

const int Painter::shadowMapSize = 1024;
const int Painter::downsamplingStepForBloom = 1;
const int Painter::bloomMapSize = 1 << (Painter::downsamplingPassesCount - 1 - Painter::downsamplingStepForBloom);

//*** Painter::Hasher

size_t Painter::Hasher::operator()(const LightVariantKey& key) const
{
	return key.basicLightsCount | (key.shadowLightsCount << 3);
}

size_t Painter::Hasher::operator()(const VertexShaderKey& key) const
{
	return (size_t)key.instanced | ((size_t)key.skinned << 1) | ((size_t)key.decal << 2);
}

size_t Painter::Hasher::operator()(const PixelShaderKey& key) const
{
	return key.basicLightsCount | (key.shadowLightsCount << 3) | ((size_t)key.decal << 6) | ((*this)(key.materialKey) << 7);
}

size_t Painter::Hasher::operator()(const MaterialKey& key) const
{
	return (size_t)key.hasDiffuseTexture | ((size_t)key.hasSpecularTexture << 1) | ((size_t)key.hasNormalTexture << 2) | ((size_t)key.useEnvironment << 3);
}

//*** Painter::BasicLight

Painter::BasicLight::BasicLight(ptr<UniformGroup> ug) :
	uLightPosition(ug->AddUniform<vec3>()),
	uLightColor(ug->AddUniform<vec3>())
{}

//*** Painter::ShadowLight

Painter::ShadowLight::ShadowLight(ptr<UniformGroup> ug, int samplerNumber) :
	BasicLight(ug),
	uLightTransform(ug->AddUniform<mat4x4>()),
	uShadowSampler(samplerNumber)
{}

// Painter::LightVariant

Painter::LightVariant::LightVariant() :
	ugLight(NEW(UniformGroup(1))),
	uAmbientColor(ugLight->AddUniform<vec3>())
{}

bool operator==(const Painter::LightVariantKey& a, const Painter::LightVariantKey& b)
{
	return
		a.basicLightsCount == b.basicLightsCount &&
		a.shadowLightsCount == b.shadowLightsCount;
}

//*** Painter::VertexShaderKey

Painter::VertexShaderKey::VertexShaderKey(bool instanced, bool skinned, bool decal)
: instanced(instanced), skinned(skinned), decal(decal) {}

bool operator==(const Painter::VertexShaderKey& a, const Painter::VertexShaderKey& b)
{
	return
		a.instanced == b.instanced &&
		a.skinned == b.skinned &&
		a.decal == b.decal;
}

//*** Painter::PixelShaderKey

Painter::PixelShaderKey::PixelShaderKey(int basicLightsCount, int shadowLightsCount, bool decal, const MaterialKey& materialKey) :
basicLightsCount(basicLightsCount), shadowLightsCount(shadowLightsCount), decal(decal), materialKey(materialKey)
{}

bool operator==(const Painter::PixelShaderKey& a, const Painter::PixelShaderKey& b)
{
	return
		a.basicLightsCount == b.basicLightsCount &&
		a.shadowLightsCount == b.shadowLightsCount &&
		a.decal == b.decal &&
		a.materialKey == b.materialKey;
}

//*** Painter::Model

Painter::Model::Model(ptr<Material> material, ptr<Geometry> geometry, const mat4x4& worldTransform)
: material(material), geometry(geometry), worldTransform(worldTransform) {}

//*** Painter::SkinnedModel

Painter::SkinnedModel::SkinnedModel(ptr<Material> material, ptr<Geometry> geometry, ptr<Geometry> shadowGeometry, ptr<BoneAnimationFrame> animationFrame)
: material(material), geometry(geometry), shadowGeometry(shadowGeometry), animationFrame(animationFrame) {}

//*** Painter::Decal

Painter::Decal::Decal(ptr<Material> material, const mat4x4& transform, const mat4x4& invTransform)
: material(material), transform(transform), invTransform(invTransform) {}

//*** Painter::Light

Painter::Light::Light(const vec3& position, const vec3& color)
: position(position), color(color), shadow(false) {}

Painter::Light::Light(const vec3& position, const vec3& color, const mat4x4& transform)
: position(position), color(color), transform(transform), shadow(true) {}

//*** Painter::DecalStuff

Painter::DecalStuff::DecalStuff(ptr<Device> device) :
	vl(NEW(VertexLayout(sizeof(Vertex)))),
	al(NEW(AttributeLayout())),
	als(al->AddSlot()),
	aPosition(al->AddElement(als, vl->AddElement(&Vertex::position))),
	aNormal(al->AddElement(als, vl->AddElement(&Vertex::normal))),
	aTexcoord(al->AddElement(als, vl->AddElement(&Vertex::texcoord)))
{
	Vertex vertices[] =
	{
		{ vec4(0, 0, 0, 1), vec3(0, 0, 1), vec2(0, 0) },
		{ vec4(1, 0, 0, 1), vec3(0, 0, 1), vec2(1, 0) },
		{ vec4(1, 1, 0, 1), vec3(0, 0, 1), vec2(1, 1) },
		{ vec4(0, 1, 0, 1), vec3(0, 0, 1), vec2(0, 1) },
		{ vec4(0, 0, 1, 1), vec3(0, 0, 1), vec2(0, 0) },
		{ vec4(1, 0, 1, 1), vec3(0, 0, 1), vec2(1, 0) },
		{ vec4(1, 1, 1, 1), vec3(0, 0, 1), vec2(1, 1) },
		{ vec4(0, 1, 1, 1), vec3(0, 0, 1), vec2(0, 1) }
	};
	unsigned short indices[] =
	{
		0, 2, 1, 0, 3, 2,
		0, 1, 5, 0, 5, 4,
		1, 2, 6, 1, 6, 5,
		2, 3, 7, 2, 7, 6,
		3, 0, 4, 3, 4, 7
	};

	vb = device->CreateStaticVertexBuffer(MemoryFile::CreateViaCopy(vertices, sizeof(vertices)), vl);
	ib = device->CreateStaticIndexBuffer(MemoryFile::CreateViaCopy(indices, sizeof(indices)), sizeof(unsigned short));

	ab = device->CreateAttributeBinding(al);

	// состояние смешивания для декалей
	bs = device->CreateBlendState();
	bs->SetColor(BlendState::colorSourceSrcAlpha, BlendState::colorSourceInvSrcAlpha, BlendState::operationAdd);
}

//*** Painter

Painter::Painter(ptr<Device> device, ptr<Context> context, ptr<Presenter> presenter, int screenWidth, int screenHeight, ptr<ShaderCache> shaderCache, ptr<GeometryFormats> geometryFormats) :
	device(device),
	context(context),
	presenter(presenter),
	screenWidth(screenWidth),
	screenHeight(screenHeight),
	shaderCache(shaderCache),
	geometryFormats(geometryFormats),

	ab(device->CreateAttributeBinding(geometryFormats->al)),
	instancer(NEW(Instancer(device, maxInstancesCount, geometryFormats->al))),
	abInstanced(device->CreateAttributeBinding(geometryFormats->al)),
	aPosition(geometryFormats->alePosition),
	aNormal(geometryFormats->aleNormal),
	aTexcoord(geometryFormats->aleTexcoord),
	abSkinned(device->CreateAttributeBinding(geometryFormats->alSkinned)),
	aSkinnedPosition(geometryFormats->aleSkinnedPosition),
	aSkinnedNormal(geometryFormats->aleSkinnedNormal),
	aSkinnedTexcoord(geometryFormats->aleSkinnedTexcoord),
	aSkinnedBoneNumbers(geometryFormats->aleSkinnedBoneNumbers),
	aSkinnedBoneWeights(geometryFormats->aleSkinnedBoneWeights),

	ugCamera(NEW(UniformGroup(0))),
	uViewProj(ugCamera->AddUniform<mat4x4>()),
	uInvViewProj(ugCamera->AddUniform<mat4x4>()),
	uCameraPosition(ugCamera->AddUniform<vec3>()),

	ugMaterial(NEW(UniformGroup(2))),
	uDiffuse(ugMaterial->AddUniform<vec4>()),
	uSpecular(ugMaterial->AddUniform<vec4>()),
	uNormalCoordTransform(ugMaterial->AddUniform<vec4>()),
	uDiffuseSampler(0),
	uSpecularSampler(1),
	uNormalSampler(2),
	uEnvironmentSampler(3),

	ugModel(NEW(UniformGroup(3))),
	uWorld(ugModel->AddUniform<mat4x4>()),

	ugInstancedModel(NEW(UniformGroup(3))),
	uWorlds(ugInstancedModel->AddUniformArray<mat4x4>(maxInstancesCount)),

	ugSkinnedModel(NEW(UniformGroup(3))),
	uBoneOrientations(ugSkinnedModel->AddUniformArray<vec4>(maxBonesCount)),
	uBoneOffsets(ugSkinnedModel->AddUniformArray<vec4>(maxBonesCount)),

	ugDecal(NEW(UniformGroup(3))),
	uDecalTransforms(ugDecal->AddUniformArray<mat4x4>(maxDecalsCount)),
	uDecalInvTransforms(ugDecal->AddUniformArray<mat4x4>(maxDecalsCount)),
	uScreenNormalSampler(3),
	uScreenDepthSampler(4),

	ugShadowBlur(NEW(UniformGroup(0))),
	uShadowBlurDirection(ugShadowBlur->AddUniform<vec2>()),
	uShadowBlurSourceSampler(0),

	ugDownsample(NEW(UniformGroup(0))),
	uDownsampleOffsets(ugDownsample->AddUniform<vec4>()),
	uDownsampleBlend(ugDownsample->AddUniform<float>()),
	uDownsampleSourceSampler(0),
	uDownsampleLuminanceSourceSampler(0),

	ugBloom(NEW(UniformGroup(0))),
	uBloomLimit(ugBloom->AddUniform<float>()),
	uBloomSourceSampler(0),

	ugTone(NEW(UniformGroup(0))),
	uToneLuminanceKey(ugTone->AddUniform<float>()),
	uToneMaxLuminance(ugTone->AddUniform<float>()),
	uToneBloomSampler(0),
	uToneScreenSampler(1),
	uToneAverageSampler(2),

	iNormal(0),
	iTexcoord(1),
	iWorldPosition(2),
	iDepth(3),
	iScreen(4),
	iInstance(5),

	decalStuff(device)

{
	// финализировать uniform группы
	ugCamera->Finalize(device);
	ugMaterial->Finalize(device);
	ugModel->Finalize(device);
	ugInstancedModel->Finalize(device);
	ugSkinnedModel->Finalize(device);
	ugDecal->Finalize(device);
	ugShadowBlur->Finalize(device);
	ugDownsample->Finalize(device);
	ugBloom->Finalize(device);
	ugTone->Finalize(device);

	// создать ресурсы
	// запомнить размеры
	this->screenWidth = screenWidth;
	this->screenHeight = screenHeight;

	//** создать ресурсы
	dsbDepth = device->CreateDepthStencilBuffer(screenWidth, screenHeight, true);
	dsbShadow = device->CreateDepthStencilBuffer(shadowMapSize, shadowMapSize, false);
	for(int i = 0; i < maxShadowLightsCount; ++i)
	{
		ptr<RenderBuffer> rb = device->CreateRenderBuffer(shadowMapSize, shadowMapSize, PixelFormats::floatR16);
		ptr<FrameBuffer> fb = device->CreateFrameBuffer();
		fb->SetColorBuffer(0, rb);
		fb->SetDepthStencilBuffer(dsbShadow);
		ptr<FrameBuffer> fbBlur = device->CreateFrameBuffer();
		fbBlur->SetColorBuffer(0, rb);
		rbShadows[i] = rb;
		fbShadows[i] = fb;
		fbShadowBlurs[i] = fbBlur;
	}
	rbShadowBlur = device->CreateRenderBuffer(shadowMapSize, shadowMapSize, PixelFormats::floatR16);

	// экранный буфер
	rbScreen = device->CreateRenderBuffer(screenWidth, screenHeight, PixelFormats::floatRGB32);
	// экранный буфер нормалей
	rbScreenNormal = device->CreateRenderBuffer(screenWidth, screenHeight, PixelFormats::floatRGB32);
	// буферы для downsample
	for(int i = 0; i < downsamplingPassesCount; ++i)
	{
		ptr<RenderBuffer> rb = device->CreateRenderBuffer(1 << (downsamplingPassesCount - 1 - i), 1 << (downsamplingPassesCount - 1 - i),
			i <= downsamplingStepForBloom ? PixelFormats::floatRGB32 : PixelFormats::floatR16);
		rbDownsamples[i] = rb;
		ptr<FrameBuffer> fb = device->CreateFrameBuffer();
		fb->SetColorBuffer(0, rb);
		fbDownsamples[i] = fb;
	}
	// буферы для Bloom
	rbBloom1 = device->CreateRenderBuffer(bloomMapSize, bloomMapSize, PixelFormats::floatRGB32);
	rbBloom2 = device->CreateRenderBuffer(bloomMapSize, bloomMapSize, PixelFormats::floatRGB32);

	// framebuffers
	fbOpaque = device->CreateFrameBuffer();
	fbOpaque->SetColorBuffer(0, rbScreen);
	fbOpaque->SetColorBuffer(1, rbScreenNormal);
	fbOpaque->SetDepthStencilBuffer(dsbDepth);
	fbDecal = device->CreateFrameBuffer();
	fbDecal->SetColorBuffer(0, rbScreen);

	shadowSamplerState = device->CreateSamplerState();
	shadowSamplerState->SetWrap(SamplerState::wrapBorder, SamplerState::wrapBorder, SamplerState::wrapBorder);
	shadowSamplerState->SetFilter(SamplerState::filterLinear, SamplerState::filterLinear, SamplerState::filterLinear);
	{
		float borderColor[] = { 0, 0, 0, 0 };
		shadowSamplerState->SetBorderColor(borderColor);
	}

	// геометрия полноэкранного прохода
	struct Quad
	{
		// вершина для фильтра
		struct Vertex
		{
			vec4 position;
			vec2 texcoord;
			vec2 gap;
		};

		ptr<VertexLayout> vl;
		ptr<AttributeLayout> al;
		ptr<AttributeLayoutSlot> als;
		Value<vec4> aPosition;
		Value<vec2> aTexcoord;

		ptr<VertexBuffer> vb;
		ptr<IndexBuffer> ib;

		ptr<AttributeBinding> ab;

		Quad(ptr<Device> device) :
			vl(NEW(VertexLayout(sizeof(Vertex)))),
			al(NEW(AttributeLayout())),
			als(al->AddSlot()),
			aPosition(al->AddElement(als, vl->AddElement(&Vertex::position))),
			aTexcoord(al->AddElement(als, vl->AddElement(&Vertex::texcoord)))
		{
			// разметка геометрии
			// геометрия полноэкранного квадрата
			Vertex vertices[] =
			{
				{ vec4(-1, -1, 0, 1), vec2(0, 1) },
				{ vec4(1, -1, 0, 1), vec2(1, 1) },
				{ vec4(1, 1, 0, 1), vec2(1, 0) },
				{ vec4(-1, 1, 0, 1), vec2(0, 0) }
			};
			unsigned short indices[] = { 0, 2, 1, 0, 3, 2 };

			vb = device->CreateStaticVertexBuffer(MemoryFile::CreateViaCopy(vertices, sizeof(vertices)), vl);
			ib = device->CreateStaticIndexBuffer(MemoryFile::CreateViaCopy(indices, sizeof(indices)), sizeof(unsigned short));

			ab = device->CreateAttributeBinding(al);
		}
	} quad(device);

	//** инициализировать состояния конвейера

	// пиксельный шейдер для теней
	psShadow = shaderCache->GetPixelShader(Expression((
		iDepth,
		fragment(0, newvec4(iDepth, 0, 0, 0))
		)));

	//** шейдеры и состояния постпроцессинга и размытия теней
	abFilter = quad.ab;
	vbFilter = quad.vb;
	ibFilter = quad.ib;

	{
		// промежуточные
		Interpolant<vec2> iTexcoord(0);

		// вершинный шейдер - общий для всех постпроцессингов
		vsFilter = shaderCache->GetVertexShader((
			setPosition(quad.aPosition),
			iTexcoord = screenToTexture(quad.aPosition["xy"])
			));

		// пиксельный шейдер для размытия тени
		{
			Temp<float> sum;
			Expression shader = (
				iTexcoord,
				sum = 0
				);
			static const float taps[] = { 0.006f, 0.061f, 0.242f, 0.383f, 0.242f, 0.061f, 0.006f };
			for(int i = 0; i < sizeof(taps) / sizeof(taps[0]); ++i)
				shader.Append((
					sum = sum + exp(uShadowBlurSourceSampler.Sample(iTexcoord + uShadowBlurDirection * Value<float>((float)i - 3))) * Value<float>(taps[i])
					));
			shader.Append((
				fragment(0, newvec4(log(sum), 0, 0, 1))
				));
			psShadowBlur = shaderCache->GetPixelShader(shader);
		}

		// пиксельный шейдер для downsample
		{
			Expression shader = (
				iTexcoord,
				fragment(0, newvec4((
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xz"]) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xw"]) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yz"]) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yw"])
					) * Value<float>(0.25f), 1.0f))
				);
			psDownsample = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для первого downsample luminance
		{
			Temp<vec3> luminanceCoef;
			Expression shader = (
				iTexcoord,
				luminanceCoef = newvec3(0.2126f, 0.7152f, 0.0722f),
				fragment(0, newvec4((
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xz"]), luminanceCoef) + Value<float>(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xw"]), luminanceCoef) + Value<float>(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yz"]), luminanceCoef) + Value<float>(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yw"]), luminanceCoef) + Value<float>(0.0001f))
					) * Value<float>(0.25f), 0.0f, 0.0f, 1.0f))
				);
			psDownsampleLuminanceFirst = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для downsample luminance
		{
			Expression shader = (
				iTexcoord,
				fragment(0, newvec4((
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xz"]) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xw"]) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yz"]) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yw"])
					) * Value<float>(0.25f), 0.0f, 0.0f, uDownsampleBlend))
				);
			psDownsampleLuminance = shaderCache->GetPixelShader(shader);
		}

		// точки для шейдера
		//const float offsets[] = { -7, -3, -1, 0, 1, 3, 7 };
		const float offsets[] = { -7, -5.9f, -3.2f, -2.1f, -1.1f, -0.5f, 0, 0.5f, 1.1f, 2.1f, 3.2f, 5.9f, 7 };
		const float offsetScaleX = 1.0f / bloomMapSize, offsetScaleY = 1.0f / bloomMapSize;
		// пиксельный шейдер для самого первого прохода (с ограничением по освещённости)
		{
			Temp<vec3> sum;
			Expression shader = (
				iTexcoord,
				sum = newvec3(0, 0, 0)
				);
			for(int i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
			{
				shader.Append((
					sum = sum + max(uBloomSourceSampler.Sample(iTexcoord + newvec2(offsets[i] * offsetScaleX, 0)) - uBloomLimit, newvec3(0, 0, 0))
					));
			}
			shader.Append((
				fragment(0, newvec4(sum * Value<float>(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f))
				));
			psBloomLimit = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для первого прохода
		{
			Temp<vec3> sum;
			Expression shader = (
				iTexcoord,
				sum = newvec3(0, 0, 0)
				);
			for(int i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
			{
				shader.Append((
					sum = sum + uBloomSourceSampler.Sample(iTexcoord + newvec2(offsets[i] * offsetScaleX, 0))
					));
			}
			shader.Append((
				fragment(0, newvec4(sum * Value<float>(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f))
				));
			psBloom1 = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для второго прохода
		{
			Temp<vec3> sum;
			Expression shader = (
				iTexcoord,
				sum = newvec3(0, 0, 0)
				);
			for(int i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
			{
				shader.Append((
					sum = sum + uBloomSourceSampler.Sample(iTexcoord + newvec2(0, offsets[i] * offsetScaleY))
					));
			}
			shader.Append((
				fragment(0, newvec4(sum * Value<float>(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f))
				));
			psBloom2 = shaderCache->GetPixelShader(shader);
		}
		// шейдер tone mapping
		{
			Temp<vec3> color;
			Temp<float> luminance, relativeLuminance, intensity;
			Expression shader = (
				iTexcoord,
				color = uToneScreenSampler.Sample(iTexcoord) + uToneBloomSampler.Sample(iTexcoord),
				luminance = dot(color, newvec3(0.2126f, 0.7152f, 0.0722f)),
				relativeLuminance = uToneLuminanceKey * luminance / exp(uToneAverageSampler.Sample(newvec2(0.5f, 0.5f))),
				intensity = relativeLuminance * (Value<float>(1) + relativeLuminance / uToneMaxLuminance) / (Value<float>(1) + relativeLuminance),
				color = saturate(color * (intensity / luminance)),
				// гамма-коррекция
				color = pow(color, newvec3(0.45f, 0.45f, 0.45f)),
				fragment(0, newvec4(color, 1.0f))
			);
			psTone = shaderCache->GetPixelShader(shader);
		}

		// color texture sampler
		ssColorTexture = device->CreateSamplerState();
		ssColorTexture->SetFilter(SamplerState::filterLinear, SamplerState::filterLinear, SamplerState::filterLinear);
		ssColorTexture->SetWrap(SamplerState::wrapRepeat, SamplerState::wrapRepeat, SamplerState::wrapRepeat);
		ssColorTexture->SetMipMapping(true);
		// point sampler
		ssPoint = device->CreateSamplerState();
		ssPoint->SetFilter(SamplerState::filterPoint, SamplerState::filterPoint, SamplerState::filterPoint);
		ssPoint->SetWrap(SamplerState::wrapClamp, SamplerState::wrapClamp, SamplerState::wrapClamp);
		// linear sampler
		ssLinear = device->CreateSamplerState();
		ssLinear->SetFilter(SamplerState::filterLinear, SamplerState::filterLinear, SamplerState::filterLinear);
		ssLinear->SetWrap(SamplerState::wrapClamp, SamplerState::wrapClamp, SamplerState::wrapClamp);
		// point sampler with border=0
		ssPointBorder = device->CreateSamplerState();
		ssPointBorder->SetFilter(SamplerState::filterPoint, SamplerState::filterPoint, SamplerState::filterPoint);
		ssPointBorder->SetWrap(SamplerState::wrapBorder, SamplerState::wrapBorder, SamplerState::wrapBorder);
		float borderColor[] = { 0, 0, 0, 0 };
		ssPointBorder->SetBorderColor(borderColor);

		// фреймбуферы для размытия тени
		fbShadowBlur1 = device->CreateFrameBuffer();
		fbShadowBlur1->SetColorBuffer(0, rbShadowBlur);

		// для последнего прохода - специальный blend state
		bsLastDownsample = device->CreateBlendState();
		bsLastDownsample->SetColor(BlendState::colorSourceSrcAlpha, BlendState::colorSourceInvSrcAlpha, BlendState::operationAdd);

		// фреймбуферы для bloom
		fbBloom1 = device->CreateFrameBuffer();
		fbBloom1->SetColorBuffer(0, rbBloom1);
		fbBloom2 = device->CreateFrameBuffer();
		fbBloom2->SetColorBuffer(0, rbBloom2);
	}
}

Painter::LightVariant& Painter::GetLightVariant(const LightVariantKey& key)
{
	// если он уже есть в кэше, вернуть
	{
		std::unordered_map<LightVariantKey, LightVariant, Hasher>::iterator i = lightVariantsCache.find(key);
		if(i != lightVariantsCache.end())
			return i->second;
	}

	// создаём новый
	int basicLightsCount = key.basicLightsCount;
	int shadowLightsCount = key.shadowLightsCount;

	LightVariant lightVariant;

	// инициализировать uniform'ы
	for(int i = 0; i < basicLightsCount; ++i)
		lightVariant.basicLights.push_back(BasicLight(lightVariant.ugLight));
	for(int i = 0; i < shadowLightsCount; ++i)
		// первые 5 семплеров пропустить
		lightVariant.shadowLights.push_back(ShadowLight(lightVariant.ugLight, i + 5));

	lightVariant.ugLight->Finalize(device);

	// добавить вариант
	lightVariantsCache.insert(std::make_pair(key, lightVariant));

	// вернуть
	return lightVariantsCache.find(key)->second;
}

Value<vec3> Painter::ApplyQuaternion(Value<vec4> q, Value<vec3> v)
{
	return v + cross(q["xyz"], cross(q["xyz"], v) + v * q["w"]) * Value<float>(2);
}

Expression Painter::GetWorldPositionAndNormal(const VertexShaderKey& key)
{
	if(key.skinned)
	{
		Value<vec3> position = aSkinnedPosition;
		Value<uint> boneNumbers[] =
		{
			aSkinnedBoneNumbers["x"],
			aSkinnedBoneNumbers["y"],
			aSkinnedBoneNumbers["z"],
			aSkinnedBoneNumbers["w"]
		};
		Value<float> boneWeights[] =
		{
			aSkinnedBoneWeights["x"],
			aSkinnedBoneWeights["y"],
			aSkinnedBoneWeights["z"],
			aSkinnedBoneWeights["w"]
		};
		Temp<vec3> tmpBoneOffsets[4];

		return
			tmpBoneOffsets[0] = uBoneOffsets[boneNumbers[0]]["xyz"],
			tmpBoneOffsets[1] = uBoneOffsets[boneNumbers[1]]["xyz"],
			tmpBoneOffsets[2] = uBoneOffsets[boneNumbers[2]]["xyz"],
			tmpBoneOffsets[3] = uBoneOffsets[boneNumbers[3]]["xyz"],
			tmpVertexPosition = newvec4(
				(ApplyQuaternion(uBoneOrientations[boneNumbers[0]], position) + tmpBoneOffsets[0]) * boneWeights[0] +
				(ApplyQuaternion(uBoneOrientations[boneNumbers[1]], position) + tmpBoneOffsets[1]) * boneWeights[1] +
				(ApplyQuaternion(uBoneOrientations[boneNumbers[2]], position) + tmpBoneOffsets[2]) * boneWeights[2] +
				(ApplyQuaternion(uBoneOrientations[boneNumbers[3]], position) + tmpBoneOffsets[3]) * boneWeights[3],
				1.0f),
			tmpVertexNormal =
				ApplyQuaternion(uBoneOrientations[boneNumbers[0]], aSkinnedNormal) * boneWeights[0] +
				ApplyQuaternion(uBoneOrientations[boneNumbers[1]], aSkinnedNormal) * boneWeights[1] +
				ApplyQuaternion(uBoneOrientations[boneNumbers[2]], aSkinnedNormal) * boneWeights[2] +
				ApplyQuaternion(uBoneOrientations[boneNumbers[3]], aSkinnedNormal) * boneWeights[3];
	}
	else
	{
		Temp<mat4x4> tmpWorld;
		Temp<vec4> tmpPosition;
		Temp<uint> tmpInstance;
		Expression e((
			key.instanced ?
			(
				tmpInstance = instancer->GetInstanceID(),
				(
					key.decal ?
					(
						iInstance = tmpInstance,
						tmpWorld = uDecalInvTransforms[tmpInstance],
						tmpPosition = mul(tmpWorld, decalStuff.aPosition),
						tmpPosition = tmpPosition / tmpPosition["w"]
					)
					:
					(
						tmpWorld = uWorlds[tmpInstance],
						tmpPosition = mul(tmpWorld, newvec4(aPosition, 1.0f))
					)
				)
			)
			:
			(
				tmpWorld = uWorld,
				tmpPosition = mul(tmpWorld, newvec4(aPosition, 1.0f))
			)
		));

		return
			e,
			tmpVertexPosition = Value<vec4>(tmpPosition),
			tmpVertexNormal = mul(tmpWorld.Cast<mat3x3>(), key.decal ? decalStuff.aNormal : aNormal);
	}
}

Expression Painter::BeginMaterialLighting(const PixelShaderKey& key, Value<vec3> ambientColor)
{
	Expression e =
		tmpWorldPosition = newvec4(iWorldPosition, 1.0f);

	// получить текстурные координаты
	if(key.decal)
	{
		Temp<vec4> tmpScreen, tmpProjectedPosition;
		Temp<vec2> tmpScreenCoords;
		Temp<float> tmpScreenDepth;
		e.Append((
			// получить спроецированную позицию
			tmpScreen = iScreen / iScreen["w"],
			tmpScreenCoords = newvec2(
				tmpScreen["x"] + Value<float>(1),
				- tmpScreen["y"] + Value<float>(1)) * Value<float>(0.5f),
			tmpScreenDepth = uScreenDepthSampler.Sample(tmpScreenCoords),
			tmpProjectedPosition = mul(uInvViewProj, newvec4(tmpScreen["xy"], tmpScreenDepth, 1)),
			tmpProjectedPosition = tmpProjectedPosition / tmpProjectedPosition["w"],
			// преобразовать эту позицию в пространство декали
			tmpProjectedPosition = mul(uDecalTransforms[iInstance], tmpProjectedPosition),
			tmpProjectedPosition = tmpProjectedPosition / tmpProjectedPosition["w"],
			//clip(tmpProjectedPosition["z"]),
			//clip(Value<float>(1) - tmpProjectedPosition["z"]),
			// получить текстурные координаты
			tmpTexcoord = newvec2(
				tmpProjectedPosition["x"] + Value<float>(1),
				-tmpProjectedPosition["y"] + Value<float>(1)) * Value<float>(0.5f),

			// получить нормаль из буфера нормалей
			tmpNormal = normalize((uScreenNormalSampler.Sample(tmpScreenCoords) * Value<float>(2)) - Value<float>(1))
		));
	}
	else
	{
		e.Append((
			tmpTexcoord = iTexcoord
		));

		// получить нормаль
		if(key.materialKey.hasNormalTexture)
		{
			Temp<vec3> dxPosition, dyPosition;
			Temp<vec2> dxTexcoord, dyTexcoord;
			Temp<vec3> r0, r1, r2, T1, T2, T3;
			Temp<vec3> perturbedNormal;
			e.Append((
				dxPosition = ddx(iWorldPosition),
				dyPosition = ddy(iWorldPosition),
				dxTexcoord = ddx(tmpTexcoord),
				dyTexcoord = ddy(tmpTexcoord),

				r0 = cross(dxPosition, dyPosition),

				r1 = cross(dyPosition, r0),
				r2 = cross(r0, dxPosition),

				T1 = normalize(r1 * dxTexcoord["x"] + r2 * dyTexcoord["x"]),
				T2 = normalize(r1 * dxTexcoord["y"] + r2 * dyTexcoord["y"]),
				T3 = normalize(iNormal),

				perturbedNormal = (uNormalSampler.Sample(tmpTexcoord * uNormalCoordTransform["xy"] + uNormalCoordTransform["zw"]) * Value<float>(2) - Value<float>(1)),
				tmpNormal = normalize(T1 * perturbedNormal["x"] + T2 * perturbedNormal["y"] + T3 * perturbedNormal["z"])
			));
		}
		else
			e.Append((
				tmpNormal = normalize(iNormal)
			));
	}

	e.Append((
		tmpToCamera = normalize(uCameraPosition - iWorldPosition),
		tmpDiffuse = key.materialKey.hasDiffuseTexture ? uDiffuseSampler.Sample(tmpTexcoord) : uDiffuse,
		tmpSpecular = key.materialKey.hasSpecularTexture ? uSpecularSampler.Sample(tmpTexcoord) : uSpecular,
		tmpSpecularExponent = exp2(tmpSpecular["x"] * Value<float>(4/*12*/)),
		tmpColor = ambientColor * tmpDiffuse["xyz"]
	));

	return e;
}

Expression Painter::ApplyMaterialLighting(Value<vec3> lightPosition, Value<vec3> lightColor)
{
	Temp<vec3> tmpToLight, tmpLightViewBissect, tmpDiffusePart, tmpSpecularPart;
	return
		// направление на свет
		tmpToLight = normalize(lightPosition - iWorldPosition),
		// биссектриса между направлениями на свет и камеру
		tmpLightViewBissect = normalize(tmpToLight + tmpToCamera),
		// диффузная составляющая
		tmpDiffusePart = tmpDiffuse["xyz"]
			* max(dot(tmpNormal, tmpToLight), Value<float>(0.0f)),
		// specular составляющая
		tmpSpecularPart = tmpDiffuse["xyz"]
			* pow(max(dot(tmpLightViewBissect, tmpNormal), Value<float>(0.0f)), tmpSpecularExponent)
			//* dot(tmpNormal, tmpToLight) // хз, может не нужно оно?
			* (tmpSpecularExponent + Value<float>(1)) / (max(pow(dot(tmpToLight, tmpLightViewBissect), Value<float>(3.0f)), Value<float>(0.1f)) * Value<float>(8)),
		// результирующая добавка к цвету
		tmpColor = tmpColor + lightColor * (tmpDiffusePart + tmpSpecularPart);
}

ptr<VertexShader> Painter::GetVertexShader(const VertexShaderKey& key)
{
	// если есть в кэше, вернуть
	{
		std::unordered_map<VertexShaderKey, ptr<VertexShader>, Hasher>::iterator i = vertexShaderCache.find(key);
		if(i != vertexShaderCache.end())
			return i->second;
	}

	// делаем новый

	Temp<vec4> p;

	Expression e((
		GetWorldPositionAndNormal(key),
		p = mul(uViewProj, tmpVertexPosition),
		setPosition(p),
		iNormal = tmpVertexNormal,
		iTexcoord = key.skinned ? aSkinnedTexcoord : aTexcoord,
		iWorldPosition = tmpVertexPosition["xyz"]
	));

	// если декали, то ещё одна штука
	if(key.decal)
		e.Append((
			iScreen = p
		));

	ptr<VertexShader> vertexShader = shaderCache->GetVertexShader(e);

	// добавить и вернуть
	vertexShaderCache.insert(std::make_pair(key, vertexShader));
	return vertexShaderCache.find(key)->second;
}

ptr<VertexShader> Painter::GetVertexShadowShader(const VertexShaderKey& key)
{
	// если есть в кэше, вернуть
	{
		std::unordered_map<VertexShaderKey, ptr<VertexShader>, Hasher>::iterator i = vertexShadowShaderCache.find(key);
		if(i != vertexShadowShaderCache.end())
			return i->second;
	}

	// делаем новый

	Temp<vec4> tmpPosition;
	ptr<VertexShader> vertexShader = shaderCache->GetVertexShader(Expression((
		GetWorldPositionAndNormal(key),
		tmpPosition = mul(uViewProj, tmpVertexPosition),
		setPosition(tmpPosition),
		iDepth = tmpPosition["z"]
		)));

	// добавить и вернуть
	vertexShadowShaderCache.insert(std::make_pair(key, vertexShader));
	return vertexShadowShaderCache.find(key)->second;
}

ptr<PixelShader> Painter::GetPixelShader(const PixelShaderKey& key)
{
	// если есть в кэше, вернуть
	{
		std::unordered_map<PixelShaderKey, ptr<PixelShader>, Hasher>::iterator i = pixelShaderCache.find(key);
		if(i != pixelShaderCache.end())
			return i->second;
	}

	// создаём новый

	int basicLightsCount = key.basicLightsCount;
	int shadowLightsCount = key.shadowLightsCount;

	// получить вариант света
	LightVariant& lightVariant = GetLightVariant(LightVariantKey(basicLightsCount, shadowLightsCount));

	// пиксельный шейдер
	Expression shader = (
		iNormal,
		iTexcoord,
		iWorldPosition,
		BeginMaterialLighting(key, lightVariant.uAmbientColor)
		);

	// учесть все простые источники света
	for(int i = 0; i < basicLightsCount; ++i)
	{
		BasicLight& basicLight = lightVariant.basicLights[i];

		shader.Append(ApplyMaterialLighting(basicLight.uLightPosition, basicLight.uLightColor));
	}

	// учесть все источники света с тенями
	for(int i = 0; i < shadowLightsCount; ++i)
	{
		ShadowLight& shadowLight = lightVariant.shadowLights[i];

		// тень
		Temp<vec4> shadowCoords;
		Temp<float> shadowMultiplier;
		Temp<vec2> shadowCoordsXY;
		Temp<float> linearShadowZ;
		Temp<float> lighted;
		shader.Append((
			shadowCoords = mul(shadowLight.uLightTransform, tmpWorldPosition),
			lighted = (shadowCoords["z"] > Value<float>(0)).Cast<float>(),
			linearShadowZ = shadowCoords["z"],
			//lighted = lighted * (linearShadowZ > Value<float>(0)),
			shadowCoords = shadowCoords / shadowCoords["w"],
			lighted = lighted * (abs(shadowCoords["x"]) < Value<float>(1)).Cast<float>() * (abs(shadowCoords["y"]) < Value<float>(1)).Cast<float>(),
			shadowCoordsXY = screenToTexture(shadowCoords["xy"]),
			shadowMultiplier = lighted * saturate(exp(Value<float>(4) * (shadowLight.uShadowSampler.Sample(shadowCoordsXY) - linearShadowZ))),
			
			ApplyMaterialLighting(shadowLight.uLightPosition, shadowLight.uLightColor * shadowMultiplier)
			));
	}

	// вернуть цвет
	shader.Append((
		fragment(0, newvec4(tmpColor, tmpDiffuse["w"]))
	));
	// если не декали, вернуть нормаль
	if(!key.decal)
		shader.Append((
			fragment(1, newvec4((tmpNormal + Value<float>(1)) * Value<float>(0.5f), 1))
		));

	ptr<PixelShader> pixelShader = shaderCache->GetPixelShader(shader);

	// добавить и вернуть
	pixelShaderCache.insert(std::make_pair(key, pixelShader));
	return pixelShaderCache.find(key)->second;
}

void Painter::BeginFrame(float frameTime)
{
	this->frameTime = frameTime;

	models.clear();
	skinnedModels.clear();
	decals.clear();
	lights.clear();
}

void Painter::SetCamera(const mat4x4& cameraViewProj, const vec3& cameraPosition)
{
	this->cameraViewProj = cameraViewProj;
	this->cameraInvViewProj = fromEigen(toEigen(cameraViewProj).inverse().eval());
	this->cameraPosition = cameraPosition;
}

void Painter::AddModel(ptr<Material> material, ptr<Geometry> geometry, const mat4x4& worldTransform)
{
	models.push_back(Model(material, geometry, worldTransform));
}

void Painter::AddSkinnedModel(ptr<Material> material, ptr<Geometry> geometry, ptr<BoneAnimationFrame> animationFrame)
{
	AddSkinnedModel(material, geometry, geometry, animationFrame);
}

void Painter::AddSkinnedModel(ptr<Material> material, ptr<Geometry> geometry, ptr<Geometry> shadowGeometry, ptr<BoneAnimationFrame> animationFrame)
{
	skinnedModels.push_back(SkinnedModel(material, geometry, shadowGeometry, animationFrame));
}

void Painter::AddDecal(ptr<Material> material, const mat4x4& transform, const mat4x4& invTransform)
{
	decals.push_back(Decal(material, transform, invTransform));
}

void Painter::SetAmbientColor(const vec3& ambientColor)
{
	this->ambientColor = ambientColor;
}

void Painter::SetEnvironmentTexture(ptr<Texture> environmentTexture)
{
	this->environmentTexture = environmentTexture;
}

void Painter::AddBasicLight(const vec3& position, const vec3& color)
{
	lights.push_back(Light(position, color));
}

void Painter::AddShadowLight(const vec3& position, const vec3& color, const mat4x4& transform)
{
	lights.push_back(Light(position, color, transform));
}

void Painter::SetupPostprocess(float bloomLimit, float toneLuminanceKey, float toneMaxLuminance)
{
	this->bloomLimit = bloomLimit;
	this->toneLuminanceKey = toneLuminanceKey;
	this->toneMaxLuminance = toneMaxLuminance;
}

void Painter::Draw()
{
	// получить количество простых и теневых источников света
	int basicLightsCount = 0;
	int shadowLightsCount = 0;
	for(size_t i = 0; i < lights.size(); ++i)
		++(lights[i].shadow ? shadowLightsCount : basicLightsCount);

	float zeroColor[] = { 0, 0, 0, 0 };
	float farColor[] = { 1e8, 1e8, 1e8, 1e8 };

	// выполнить теневые проходы
	int shadowPassNumber = 0;
	for(size_t i = 0; i < lights.size(); ++i)
		if(lights[i].shadow)
		{
			Context::LetViewport lv(context, shadowMapSize, shadowMapSize);
			Context::LetFrameBuffer lfb(context, fbShadows[shadowPassNumber]);
			Context::LetUniformBuffer lubCamera(context, ugCamera);
			Context::LetPixelShader lps(context, psShadow);

			ptr<RenderBuffer> rb = rbShadows[shadowPassNumber];

			// указать трансформацию
			uViewProj.SetValue(lights[i].transform);
			ugCamera->Upload(context);

			// очистить карту теней
			context->ClearColor(0, farColor);
			context->ClearDepth(1.0f);

			// сортировщик моделей по геометрии
			struct GeometrySorter
			{
				bool operator()(const Model& a, const Model& b) const
				{
					return a.geometry < b.geometry;
				}
				bool operator()(const SkinnedModel& a, const SkinnedModel& b) const
				{
					return a.shadowGeometry < b.shadowGeometry;
				}
			};

			//** рисуем простые модели

			// отсортировать объекты по геометрии
			std::sort(models.begin(), models.end(), GeometrySorter());

			{
				// установить привязку атрибутов
				Context::LetAttributeBinding lab(context, abInstanced);
				// установить вершинный шейдер
				Context::LetVertexShader lvs(context, GetVertexShadowShader(VertexShaderKey(true, false, false)));
				// установить константный буфер
				Context::LetUniformBuffer lubModel(context, ugInstancedModel);

				// нарисовать инстансингом с группировкой по геометрии
				for(size_t j = 0; j < models.size(); )
				{
					// количество рисуемых объектов
					int batchCount;
					for(batchCount = 1;
						batchCount < maxInstancesCount &&
						j + batchCount < models.size() &&
						models[j].geometry == models[j + batchCount].geometry;
						++batchCount);

					// установить геометрию
					Context::LetVertexBuffer lvb(context, 0, models[j].geometry->GetVertexBuffer());
					Context::LetIndexBuffer lib(context, models[j].geometry->GetIndexBuffer());
					// установить uniform'ы
					for(int k = 0; k < batchCount; ++k)
						uWorlds.SetValue(k, models[j + k].worldTransform);
					// и залить в GPU
					ugInstancedModel->Upload(context);

					// нарисовать
					instancer->Draw(context, batchCount);

					j += batchCount;
				}
			}

			//** рисуем skinned-модели

			// отсортировать объекты по геометрии
			std::sort(skinnedModels.begin(), skinnedModels.end(), GeometrySorter());

			{
				// установить привязку атрибутов
				Context::LetAttributeBinding lab(context, abSkinned);
				// установить вершинный шейдер
				Context::LetVertexShader lvs(context, GetVertexShadowShader(VertexShaderKey(false, true, false)));
				// установить константный буфер
				Context::LetUniformBuffer lubModel(context, ugSkinnedModel);

				// нарисовать с группировкой по геометрии
				for(size_t j = 0; j < skinnedModels.size(); ++j)
				{
					const SkinnedModel& skinnedModel = skinnedModels[j];
					// установить геометрию
					Context::LetVertexBuffer lvb(context, 0, skinnedModel.shadowGeometry->GetVertexBuffer());
					Context::LetIndexBuffer lib(context, skinnedModel.shadowGeometry->GetIndexBuffer());
					// установить uniform'ы костей
					ptr<BoneAnimationFrame> animationFrame = skinnedModel.animationFrame;
					const std::vector<quat>& orientations = animationFrame->orientations;
					const std::vector<vec3>& offsets = animationFrame->offsets;
					int bonesCount = (int)orientations.size();
#ifdef _DEBUG
					if(bonesCount > maxBonesCount)
						THROW("Too many bones");
#endif
					for(int k = 0; k < bonesCount; ++k)
					{
						uBoneOrientations.SetValue(k, orientations[k]);
						uBoneOffsets.SetValue(k, vec4(offsets[k].x, offsets[k].y, offsets[k].z, 0));
					}
					// и залить в GPU
					ugSkinnedModel->Upload(context);

					// нарисовать
					context->Draw();
				}
			}

			// выполнить размытие тени
			{
				Context::LetViewport lv(context, shadowMapSize, shadowMapSize);
				Context::LetAttributeBinding lab(context, abFilter);
				Context::LetVertexBuffer lvb(context, 0, vbFilter);
				Context::LetIndexBuffer lib(context, ibFilter);
				Context::LetVertexShader lvs(context, vsFilter);
				Context::LetPixelShader lps(context, psShadowBlur);
				Context::LetDepthTestFunc ldtf(context, Context::depthTestFuncAlways);
				Context::LetDepthWrite ldw(context, false);

				// первый проход

				{
					Context::LetFrameBuffer lfb(context, fbShadowBlur1);
					Context::LetSampler ls(context, uShadowBlurSourceSampler, rb->GetTexture(), ssPointBorder);
					Context::LetUniformBuffer lub(context, ugShadowBlur);

					uShadowBlurDirection.SetValue(vec2(1.0f / shadowMapSize, 0));
					ugShadowBlur->Upload(context);

					context->ClearColor(0, zeroColor);
					context->Draw();
				}

				// второй проход
				{
					Context::LetFrameBuffer lfb(context, fbShadowBlurs[i]);
					Context::LetSampler ls(context, uShadowBlurSourceSampler, rbShadowBlur->GetTexture(), ssPointBorder);
					Context::LetUniformBuffer lub(context, ugShadowBlur);

					uShadowBlurDirection.SetValue(vec2(0, 1.0f / shadowMapSize));
					ugShadowBlur->Upload(context);

					context->ClearColor(0, zeroColor);
					context->Draw();
				}
			}

			shadowPassNumber++;
		}

	// основное рисование

	// сортировщик моделей по материалу, а затем по геометрии
	struct Sorter
	{
		bool operator()(const Model& a, const Model& b) const
		{
			return a.material < b.material || (a.material == b.material && a.geometry < b.geometry);
		}
		bool operator()(const SkinnedModel& a, const SkinnedModel& b) const
		{
			return a.material < b.material || (a.material == b.material && a.geometry < b.geometry);
		}
		// для декалей - только по материалу
		bool operator()(const Decal& a, const Decal& b) const
		{
			return a.material < b.material;
		}
	};

	{
		Context::LetFrameBuffer lfb(context, fbOpaque);
		Context::LetViewport lv(context, screenWidth, screenHeight);
		Context::LetDepthTestFunc ldtf(context, Context::depthTestFuncLess);
		Context::LetDepthWrite ldw(context, true);
		Context::LetUniformBuffer lubCamera(context, ugCamera);

		// установить uniform'ы камеры
		uViewProj.SetValue(cameraViewProj);
		uInvViewProj.SetValue(cameraInvViewProj);
		uCameraPosition.SetValue(cameraPosition);
		ugCamera->Upload(context);

		// установить параметры источников света
		LightVariant& lightVariant = GetLightVariant(LightVariantKey(basicLightsCount, shadowLightsCount));
		Context::LetUniformBuffer lubLight(context, lightVariant.ugLight);

		lightVariant.uAmbientColor.SetValue(ambientColor);
		int basicLightNumber = 0;
		int shadowLightNumber = 0;
		Context::LetSampler ls[maxShadowLightsCount];
		for(size_t i = 0; i < lights.size(); ++i)
			if(lights[i].shadow)
			{
				ShadowLight& shadowLight = lightVariant.shadowLights[shadowLightNumber];
				shadowLight.uLightPosition.SetValue(lights[i].position);
				shadowLight.uLightColor.SetValue(lights[i].color);
				shadowLight.uLightTransform.SetValue(lights[i].transform);

				ls[shadowLightNumber](context, shadowLight.uShadowSampler, rbShadows[shadowLightNumber]->GetTexture(), shadowSamplerState);

				shadowLightNumber++;
			}
			else
			{
				BasicLight& basicLight = lightVariant.basicLights[basicLightNumber++];
				basicLight.uLightPosition.SetValue(lights[i].position);
				basicLight.uLightColor.SetValue(lights[i].color);
			}
		lightVariant.ugLight->Upload(context);

		// очистить рендербуферы
		float color[] = { 0, 0, 0, 1 };
		context->ClearColor(0, color); // color
		context->ClearColor(1, color); // normal
		context->ClearDepth(1.0f);

		//** нарисовать простые модели
		{
			std::sort(models.begin(), models.end(), Sorter());

			// установить привязку атрибутов
			Context::LetAttributeBinding lab(context, abInstanced);
			// установить вершинный шейдер
			Context::LetVertexShader lvs(context, GetVertexShader(VertexShaderKey(true, false, false)));
			// установить константный буфер
			Context::LetUniformBuffer lubModel(context, ugInstancedModel);
			// установить материал
			Context::LetUniformBuffer lubMaterial(context, ugMaterial);

			// нарисовать
			for(size_t i = 0; i < models.size(); )
			{
				// выяснить размер батча по материалу
				ptr<Material> material = models[i].material;
				int materialBatchCount;
				for(materialBatchCount = 1;
					i + materialBatchCount < models.size() &&
					material == models[i + materialBatchCount].material;
					++materialBatchCount);

				// установить параметры материала
				Context::LetSampler lsDiffuse(context, uDiffuseSampler, material->diffuseTexture, ssColorTexture);
				Context::LetSampler lsSpecular(context, uSpecularSampler, material->specularTexture, ssColorTexture);
				Context::LetSampler lsNormal(context, uNormalSampler, material->normalTexture, ssColorTexture);
				Context::LetSampler lsEnvironment(context, uEnvironmentSampler, environmentTexture, ssColorTexture);
				uDiffuse.SetValue(material->diffuse);
				uSpecular.SetValue(material->specular);
				uNormalCoordTransform.SetValue(material->normalCoordTransform);
				ugMaterial->Upload(context);

				// рисуем инстансингом обычные модели
				// установить пиксельный шейдер
				Context::LetPixelShader lps(context, GetPixelShader(PixelShaderKey(basicLightsCount, shadowLightsCount, false, material->GetKey())));
				// цикл по батчам по геометрии
				for(int j = 0; j < materialBatchCount; )
				{
					// выяснить размер батча по геометрии
					ptr<Geometry> geometry = models[i + j].geometry;
					int geometryBatchCount;
					for(geometryBatchCount = 1;
						geometryBatchCount < maxInstancesCount &&
						j + geometryBatchCount < materialBatchCount &&
						geometry == models[i + j + geometryBatchCount].geometry;
						++geometryBatchCount);

					// установить геометрию
					Context::LetVertexBuffer lvb(context, 0, geometry->GetVertexBuffer());
					Context::LetIndexBuffer lib(context, geometry->GetIndexBuffer());

					// установить uniform'ы
					for(int k = 0; k < geometryBatchCount; ++k)
						uWorlds.SetValue(k, models[i + j + k].worldTransform);
					ugInstancedModel->Upload(context);

					// нарисовать
					instancer->Draw(context, geometryBatchCount);

					j += geometryBatchCount;
				}

				i += materialBatchCount;
			}
		}

		//** нарисовать skinned-модели
		{
			std::sort(skinnedModels.begin(), skinnedModels.end(), Sorter());

			// установить привязку атрибутов
			Context::LetAttributeBinding lab(context, abSkinned);
			// установить вершинный шейдер
			Context::LetVertexShader lvs(context, GetVertexShader(VertexShaderKey(false, true, false)));
			// установить константный буфер
			Context::LetUniformBuffer lubModel(context, ugSkinnedModel);
			// установить материал
			Context::LetUniformBuffer lubMaterial(context, ugMaterial);

			// нарисовать
			for(size_t i = 0; i < skinnedModels.size(); ++i)
			{
				const SkinnedModel& skinnedModel = skinnedModels[i];

				// установить параметры материала
				ptr<Material> material = skinnedModel.material;
				Context::LetSampler lsDiffuse(context, uDiffuseSampler, material->diffuseTexture, ssColorTexture);
				Context::LetSampler lsSpecular(context, uSpecularSampler, material->specularTexture, ssColorTexture);
				Context::LetSampler lsNormal(context, uNormalSampler, material->normalTexture, ssColorTexture);
				Context::LetSampler lsEnvironment(context, uEnvironmentSampler, environmentTexture, ssColorTexture);
				uDiffuse.SetValue(material->diffuse);
				uSpecular.SetValue(material->specular);
				uNormalCoordTransform.SetValue(material->normalCoordTransform);
				ugMaterial->Upload(context);

				// установить пиксельный шейдер
				Context::LetPixelShader lps(context, GetPixelShader(PixelShaderKey(basicLightsCount, shadowLightsCount, false, material->GetKey())));

				// установить геометрию
				ptr<Geometry> geometry = skinnedModel.geometry;
				Context::LetVertexBuffer lvb(context, 0, geometry->GetVertexBuffer());
				Context::LetIndexBuffer lib(context, geometry->GetIndexBuffer());

				// установить uniform'ы костей
				ptr<BoneAnimationFrame> animationFrame = skinnedModel.animationFrame;
				const std::vector<quat>& orientations = animationFrame->orientations;
				const std::vector<vec3>& offsets = animationFrame->offsets;
				int bonesCount = (int)orientations.size();
#ifdef _DEBUG
				if(bonesCount > maxBonesCount)
					THROW("Too many bones");
#endif
				for(int k = 0; k < bonesCount; ++k)
				{
					uBoneOrientations.SetValue(k, orientations[k]);
					uBoneOffsets.SetValue(k, vec4(offsets[k].x, offsets[k].y, offsets[k].z, 0));
				}
				ugSkinnedModel->Upload(context);

				// нарисовать
				context->Draw();
			}
		}
	}

	//** нарисовать декали
	{
		std::stable_sort(decals.begin(), decals.end(), Sorter());

		// установить вершинный шейдер
		Context::LetVertexShader lvs(context, GetVertexShader(VertexShaderKey(true, false, true)));
		// установить константный буфер
		Context::LetUniformBuffer lubDecal(context, ugDecal);
		// установить материал
		Context::LetUniformBuffer lubMaterial(context, ugMaterial);
		// установить геометрию
		Context::LetAttributeBinding lab(context, decalStuff.ab);
		Context::LetVertexBuffer lvb(context, 0, decalStuff.vb);
		Context::LetIndexBuffer lib(context, decalStuff.ib);
		// состояние смешивания
		Context::LetBlendState lbs(context, decalStuff.bs);
		// убрать карту нормалей и буфер глубины
		Context::LetFrameBuffer lfb(context, fbDecal);
		// семплеры
		Context::LetSampler lsScreenNormal(context, uScreenNormalSampler, rbScreenNormal->GetTexture(), shadowSamplerState);
		Context::LetSampler lsScreenDepth(context, uScreenDepthSampler, dsbDepth->GetTexture(), shadowSamplerState);

		// нарисовать
		for(size_t i = 0; i < decals.size(); )
		{
			// выяснить размер батча по материалу
			ptr<Material> material = decals[i].material;
			int materialBatchCount;
			for(materialBatchCount = 1;
				materialBatchCount < maxDecalsCount &&
				i + materialBatchCount < decals.size() &&
				material == decals[i + materialBatchCount].material;
				++materialBatchCount);

			// установить параметры материала
			Context::LetSampler lsDiffuse(context, uDiffuseSampler, material->diffuseTexture, ssColorTexture);
			Context::LetSampler lsSpecular(context, uSpecularSampler, material->specularTexture, ssColorTexture);
			Context::LetSampler lsNormal(context, uNormalSampler, material->normalTexture, ssColorTexture);
			uDiffuse.SetValue(material->diffuse);
			uSpecular.SetValue(material->specular);
			uNormalCoordTransform.SetValue(material->normalCoordTransform);
			ugMaterial->Upload(context);

			// рисуем инстансингом декали
			// установить пиксельный шейдер
			Context::LetPixelShader lps(context, GetPixelShader(PixelShaderKey(basicLightsCount, shadowLightsCount, true, material->GetKey())));

			// установить uniform'ы
			for(int j = 0; j < materialBatchCount; ++j)
			{
				uDecalTransforms.SetValue(j, decals[i + j].transform);
				uDecalInvTransforms.SetValue(j, decals[i + j].invTransform);
			}
			ugDecal->Upload(context);

			// нарисовать
			context->DrawInstanced(materialBatchCount);

			i += materialBatchCount;
		}
	}

	// всё, теперь постпроцессинг
	{
		float clearColor[] = { 0, 0, 0, 0 };

		// общие для фильтров настройки
		Context::LetAttributeBinding lab(context, abFilter);
		Context::LetVertexBuffer lvb(context, 0, vbFilter);
		Context::LetIndexBuffer lib(context, ibFilter);
		Context::LetVertexShader lvs(context, vsFilter);
		Context::LetDepthTestFunc ldtf(context, Context::depthTestFuncAlways);
		Context::LetDepthWrite ldw(context, false);

		// downsampling
		/*
		за секунду - остаётся K
		за 2 секунды - остаётся K^2
		за t секунд - pow(K, t) = exp(t * log(K))
		*/
		static bool veryFirstDownsampling = true;
		uDownsampleBlend.SetValue(1.0f - exp(frameTime * (-0.79f)));
		for(int i = 0; i < downsamplingPassesCount; ++i)
		{
			float halfSourcePixelWidth = 0.5f / (i == 0 ? screenWidth : (1 << (downsamplingPassesCount - i)));
			float halfSourcePixelHeight = 0.5f / (i == 0 ? screenHeight : (1 << (downsamplingPassesCount - i)));
			uDownsampleOffsets.SetValue(vec4(-halfSourcePixelWidth, halfSourcePixelWidth, -halfSourcePixelHeight, halfSourcePixelHeight));
			ugDownsample->Upload(context);

			Context::LetFrameBuffer lfb(context, fbDownsamples[i]);
			Context::LetViewport lv(context, 1 << (downsamplingPassesCount - 1 - i), 1 << (downsamplingPassesCount - 1 - i));
			Context::LetUniformBuffer lub(context, ugDownsample);
			const SamplerBase* sbSampler;
			if(i <= downsamplingStepForBloom + 1)
				sbSampler = &uDownsampleSourceSampler;
			else
				sbSampler = &uDownsampleLuminanceSourceSampler;
			Context::LetSampler ls(context,
				*sbSampler,
				i == 0 ? rbScreen->GetTexture() : rbDownsamples[i - 1]->GetTexture(),
				i == 0 ? ssLinear : ssPoint
			);

			Context::LetPixelShader lps(context,
				i <= downsamplingStepForBloom ? psDownsample :
				i == downsamplingStepForBloom + 1 ? psDownsampleLuminanceFirst :
				psDownsampleLuminance
			);

			Context::LetBlendState lbs;
			if(i == downsamplingPassesCount - 1)
				lbs(context, bsLastDownsample);

			if(veryFirstDownsampling || i < downsamplingPassesCount - 1)
				context->ClearColor(0, clearColor);
			context->Draw();
		}
		veryFirstDownsampling = false;

		// bloom
		{
			uBloomLimit.SetValue(bloomLimit);
			ugBloom->Upload(context);

			const int bloomPassesCount = 5;

			bool enableBloom = true;

			Context::LetViewport lv(context, bloomMapSize, bloomMapSize);
			Context::LetUniformBuffer lub(context, ugBloom);
			if(enableBloom)
			{
				{
					Context::LetFrameBuffer lfb(context, fbBloom2);
					Context::LetSampler ls(context, uBloomSourceSampler, rbDownsamples[downsamplingStepForBloom]->GetTexture(), ssLinear);
					Context::LetPixelShader lps(context, psBloomLimit);
					context->ClearColor(0, clearColor);
					context->Draw();
				}
				{
					Context::LetFrameBuffer lfb(context, fbBloom1);
					Context::LetSampler ls(context, uBloomSourceSampler, rbBloom2->GetTexture(), ssLinear);
					Context::LetPixelShader lps(context, psBloom2);
					context->ClearColor(0, clearColor);
					context->Draw();
				}
				for(int i = 1; i < bloomPassesCount; ++i)
				{
					{
						Context::LetFrameBuffer lfb(context, fbBloom2);
						Context::LetSampler ls(context, uBloomSourceSampler, rbBloom1->GetTexture(), ssLinear);
						Context::LetPixelShader lps(context, psBloom1);
						context->ClearColor(0, clearColor);
						context->Draw();
					}
					{
						Context::LetFrameBuffer lfb(context, fbBloom1);
						Context::LetSampler ls(context, uBloomSourceSampler, rbBloom2->GetTexture(), ssLinear);
						Context::LetPixelShader lps(context, psBloom2);
						context->ClearColor(0, clearColor);
						context->Draw();
					}
				}
			}
			else
			{
				Context::LetFrameBuffer lfb(context, fbBloom1);
				Context::LetSampler ls(context, uBloomSourceSampler, rbBloom2->GetTexture(), ssLinear);
				Context::LetPixelShader lps(context, psBloom2);
				context->ClearColor(0, clearColor);
			}
		}

		// tone mapping
		{
			Context::LetFrameBuffer lfb(context, presenter->GetFrameBuffer());
			Context::LetViewport lv(context, screenWidth, screenHeight);
			Context::LetSampler lsBloom(context, uToneBloomSampler, rbBloom1->GetTexture(), ssLinear);
			Context::LetSampler lsScreen(context, uToneScreenSampler, rbScreen->GetTexture(), ssPoint);
			Context::LetSampler lsAverage(context, uToneAverageSampler, rbDownsamples[downsamplingPassesCount - 1]->GetTexture(), ssPoint);

			uToneLuminanceKey.SetValue(toneLuminanceKey);
			uToneMaxLuminance.SetValue(toneMaxLuminance);
			ugTone->Upload(context);
			Context::LetUniformBuffer lub(context, ugTone);

			Context::LetPixelShader lps(context, psTone);

			context->ClearColor(0, zeroColor);
			context->Draw();
		}
	} // postprocessing
}
