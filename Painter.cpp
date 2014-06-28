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
	return (size_t)key.instanced | ((size_t)key.skinned << 1);
}

size_t Painter::Hasher::operator()(const PixelShaderKey& key) const
{
	return key.basicLightsCount | (key.shadowLightsCount << 3) | ((*this)(key.materialKey) << 6);
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

Painter::VertexShaderKey::VertexShaderKey(bool instanced, bool skinned)
: instanced(instanced), skinned(skinned) {}

bool operator==(const Painter::VertexShaderKey& a, const Painter::VertexShaderKey& b)
{
	return
		a.instanced == b.instanced &&
		a.skinned == b.skinned;
}

//*** Painter::PixelShaderKey

Painter::PixelShaderKey::PixelShaderKey(int basicLightsCount, int shadowLightsCount, const MaterialKey& materialKey) :
basicLightsCount(basicLightsCount), shadowLightsCount(shadowLightsCount), materialKey(materialKey)
{}

bool operator==(const Painter::PixelShaderKey& a, const Painter::PixelShaderKey& b)
{
	return
		a.basicLightsCount == b.basicLightsCount &&
		a.shadowLightsCount == b.shadowLightsCount &&
		a.materialKey == b.materialKey;
}

//*** Painter::Model

Painter::Model::Model(ptr<Material> material, ptr<Geometry> geometry, const mat4x4& worldTransform)
: material(material), geometry(geometry), worldTransform(worldTransform) {}

//*** Painter::SkinnedModel

Painter::SkinnedModel::SkinnedModel(ptr<Material> material, ptr<Geometry> geometry, ptr<Geometry> shadowGeometry, ptr<BoneAnimationFrame> animationFrame)
: material(material), geometry(geometry), shadowGeometry(shadowGeometry), animationFrame(animationFrame) {}

//*** Painter::Light

Painter::Light::Light(const vec3& position, const vec3& color)
: position(position), color(color), shadow(false) {}

Painter::Light::Light(const vec3& position, const vec3& color, const mat4x4& transform)
: position(position), color(color), transform(transform), shadow(true) {}

//*** Painter

Painter::Painter(ptr<Device> device, ptr<Context> context, ptr<Presenter> presenter, ptr<ShaderCache> shaderCache, ptr<GeometryFormats> geometryFormats) :
	device(device),
	context(context),
	presenter(presenter),
	screenWidth(-1),
	screenHeight(-1),
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
	iDepth(3)

{
	// финализировать uniform группы
	ugCamera->Finalize(device);
	ugMaterial->Finalize(device);
	ugModel->Finalize(device);
	ugInstancedModel->Finalize(device);
	ugSkinnedModel->Finalize(device);
	ugShadowBlur->Finalize(device);
	ugDownsample->Finalize(device);
	ugBloom->Finalize(device);
	ugTone->Finalize(device);

	// создать ресурсы

	// создать настройки семплирования
	SamplerSettings shadowSamplerSettings;
	shadowSamplerSettings.SetWrap(SamplerSettings::wrapBorder);
	shadowSamplerSettings.SetFilter(SamplerSettings::filterLinear);
	SamplerSettings pointSamplerSettings;
	pointSamplerSettings.SetFilter(SamplerSettings::filterPoint);
	pointSamplerSettings.SetWrap(SamplerSettings::wrapClamp);

	//** создать ресурсы
	dsbShadow = device->CreateDepthStencilBuffer(shadowMapSize, shadowMapSize, false);
	for(int i = 0; i < maxShadowLightsCount; ++i)
	{
		ptr<RenderBuffer> rb = device->CreateRenderBuffer(shadowMapSize, shadowMapSize, PixelFormats::floatR16, shadowSamplerSettings);
		ptr<FrameBuffer> fb = device->CreateFrameBuffer();
		fb->SetColorBuffer(0, rb);
		fb->SetDepthStencilBuffer(dsbShadow);
		ptr<FrameBuffer> fbBlur = device->CreateFrameBuffer();
		fbBlur->SetColorBuffer(0, rb);
		rbShadows[i] = rb;
		fbShadows[i] = fb;
		fbShadowBlurs[i] = fbBlur;
	}
	rbShadowBlur = device->CreateRenderBuffer(shadowMapSize, shadowMapSize, PixelFormats::floatR16, shadowSamplerSettings);

	// буферы для downsample
	for(int i = 0; i < downsamplingPassesCount; ++i)
	{
		ptr<RenderBuffer> rb = device->CreateRenderBuffer(1 << (downsamplingPassesCount - 1 - i), 1 << (downsamplingPassesCount - 1 - i),
			i <= downsamplingStepForBloom ? PixelFormats::floatRGB32 : PixelFormats::floatR16, pointSamplerSettings);
		rbDownsamples[i] = rb;
		ptr<FrameBuffer> fb = device->CreateFrameBuffer();
		fb->SetColorBuffer(0, rb);
		fbDownsamples[i] = fb;
	}
	// буферы для Bloom
	rbBloom1 = device->CreateRenderBuffer(bloomMapSize, bloomMapSize, PixelFormats::floatRGB32, pointSamplerSettings);
	rbBloom2 = device->CreateRenderBuffer(bloomMapSize, bloomMapSize, PixelFormats::floatRGB32, pointSamplerSettings);

	shadowSamplerState = device->CreateSamplerState(shadowSamplerSettings);

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
	psShadow = shaderCache->GetPixelShader((
		fragment(0, newvec4(iDepth, 0, 0, 0))
		));

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
			iTexcoord.Set(screenToTexture(quad.aPosition["xy"]))
			));

		// пиксельный шейдер для размытия тени
		{
			Value<float> sum = 0.0f;
			static const float taps[] = { 0.006f, 0.061f, 0.242f, 0.383f, 0.242f, 0.061f, 0.006f };
			for(int i = 0; i < int(sizeof(taps) / sizeof(taps[0])); ++i)
				sum += exp(uShadowBlurSourceSampler.Sample(iTexcoord + uShadowBlurDirection * val((float)i - 3))) * val(taps[i]);
			psShadowBlur = shaderCache->GetPixelShader(
				fragment(0, newvec4(log(sum), 0, 0, 1))
			);
		}

		// пиксельный шейдер для downsample
		{
			psDownsample = shaderCache->GetPixelShader(
				fragment(0, newvec4((
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xz"]) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xw"]) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yz"]) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yw"])
				) * val(0.25f), 1.0f))
			);
		}
		// пиксельный шейдер для первого downsample luminance
		{
			Value<vec3> luminanceCoef = newvec3(0.2126f, 0.7152f, 0.0722f);
			psDownsampleLuminanceFirst = shaderCache->GetPixelShader(
				fragment(0, newvec4((
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xz"]), luminanceCoef) + val(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xw"]), luminanceCoef) + val(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yz"]), luminanceCoef) + val(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yw"]), luminanceCoef) + val(0.0001f))
				) * val(0.25f), 0.0f, 0.0f, 1.0f))
			);
		}
		// пиксельный шейдер для downsample luminance
		{
			psDownsampleLuminance = shaderCache->GetPixelShader(
				fragment(0, newvec4((
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xz"]) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xw"]) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yz"]) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yw"])
				) * val(0.25f), 0.0f, 0.0f, uDownsampleBlend))
			);
		}

		// точки для шейдера
		//const float offsets[] = { -7, -3, -1, 0, 1, 3, 7 };
		const float offsets[] = { -7, -5.9f, -3.2f, -2.1f, -1.1f, -0.5f, 0, 0.5f, 1.1f, 2.1f, 3.2f, 5.9f, 7 };
		const float offsetScaleX = 1.0f / bloomMapSize, offsetScaleY = 1.0f / bloomMapSize;
		// пиксельный шейдер для самого первого прохода (с ограничением по освещённости)
		{
			Value<vec3> sum = newvec3(0, 0, 0);
			for(size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
				sum += max(uBloomSourceSampler.Sample(iTexcoord + newvec2(offsets[i] * offsetScaleX, 0)) - uBloomLimit, newvec3(0, 0, 0));
			psBloomLimit = shaderCache->GetPixelShader(
				fragment(0, newvec4(sum * val(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f))
			);
		}
		// пиксельный шейдер для первого прохода
		{
			Value<vec3> sum = newvec3(0, 0, 0);
			for(size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
				sum += uBloomSourceSampler.Sample(iTexcoord + newvec2(offsets[i] * offsetScaleX, 0));
			psBloom1 = shaderCache->GetPixelShader(
				fragment(0, newvec4(sum * Value<float>(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f))
			);
		}
		// пиксельный шейдер для второго прохода
		{
			Value<vec3> sum = newvec3(0, 0, 0);
			for(size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
				sum += uBloomSourceSampler.Sample(iTexcoord + newvec2(0, offsets[i] * offsetScaleY));
			psBloom2 = shaderCache->GetPixelShader(
				fragment(0, newvec4(sum * Value<float>(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f))
			);
		}
		// шейдер tone mapping
		{
			Value<vec3> color = uToneScreenSampler.Sample(iTexcoord) + uToneBloomSampler.Sample(iTexcoord);
			Value<float> luminance = dot(color, newvec3(0.2126f, 0.7152f, 0.0722f));
			Value<float> relativeLuminance = uToneLuminanceKey * luminance / exp(uToneAverageSampler.Sample(newvec2(0.5f, 0.5f)));
			Value<float> intensity = relativeLuminance * (Value<float>(1) + relativeLuminance / uToneMaxLuminance) / (Value<float>(1) + relativeLuminance);
			color = saturate(color * (intensity / luminance));
			// гамма-коррекция
			color = pow(color, newvec3(0.45f, 0.45f, 0.45f));
			psTone = shaderCache->GetPixelShader(
				fragment(0, newvec4(color, 1.0f))
			);
		}

		// color texture sampler
		{
			SamplerSettings s;
			s.minFilter = s.mipFilter = s.magFilter = SamplerSettings::filterLinear;
			s.wrapU = s.wrapV = s.wrapW = SamplerSettings::wrapRepeat;
			s.mipMapping = true;
			ssColorTexture = device->CreateSamplerState(s);
		}
		// point sampler
		ssPoint = device->CreateSamplerState(pointSamplerSettings);
		// linear sampler
		{
			SamplerSettings s;
			s.minFilter = s.mipFilter = s.magFilter = SamplerSettings::filterLinear;
			s.wrapU = s.wrapV = s.wrapW = SamplerSettings::wrapClamp;
			ssLinear = device->CreateSamplerState(s);
		}
		// point sampler with border=0
		{
			SamplerSettings s;
			s.minFilter = s.mipFilter = s.magFilter = SamplerSettings::filterPoint;
			s.wrapU = s.wrapV = s.wrapW = SamplerSettings::wrapBorder;
			ssPointBorder = device->CreateSamplerState(s);
		}

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

void Painter::Resize(int screenWidth, int screenHeight)
{
	if(this->screenWidth == screenWidth && this->screenHeight == screenHeight)
		return;

	// запомнить размеры
	this->screenWidth = screenWidth;
	this->screenHeight = screenHeight;

	SamplerSettings pointSamplerSettings;
	pointSamplerSettings.SetFilter(SamplerSettings::filterPoint);
	pointSamplerSettings.SetWrap(SamplerSettings::wrapClamp);

	// main screen
	rbScreen = device->CreateRenderBuffer(screenWidth, screenHeight, PixelFormats::floatRGB32, pointSamplerSettings);
	rbScreenNormal = device->CreateRenderBuffer(screenWidth, screenHeight, PixelFormats::floatRGB32, pointSamplerSettings);
	dsbDepth = device->CreateDepthStencilBuffer(screenWidth, screenHeight, true);

	// framebuffers
	fbOpaque = device->CreateFrameBuffer();
	fbOpaque->SetColorBuffer(0, rbScreen);
	fbOpaque->SetColorBuffer(1, rbScreenNormal);
	fbOpaque->SetDepthStencilBuffer(dsbDepth);
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

void Painter::GetWorldPositionAndNormal(const VertexShaderKey& key)
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
		Value<vec3> boneOffsets[4] =
		{
			uBoneOffsets[boneNumbers[0]]["xyz"],
			uBoneOffsets[boneNumbers[1]]["xyz"],
			uBoneOffsets[boneNumbers[2]]["xyz"],
			uBoneOffsets[boneNumbers[3]]["xyz"]
		};

		tmpVertexPosition = newvec4(
			(ApplyQuaternion(uBoneOrientations[boneNumbers[0]], position) + boneOffsets[0]) * boneWeights[0] +
			(ApplyQuaternion(uBoneOrientations[boneNumbers[1]], position) + boneOffsets[1]) * boneWeights[1] +
			(ApplyQuaternion(uBoneOrientations[boneNumbers[2]], position) + boneOffsets[2]) * boneWeights[2] +
			(ApplyQuaternion(uBoneOrientations[boneNumbers[3]], position) + boneOffsets[3]) * boneWeights[3],
			1.0f);
		tmpVertexNormal =
			ApplyQuaternion(uBoneOrientations[boneNumbers[0]], aSkinnedNormal) * boneWeights[0] +
			ApplyQuaternion(uBoneOrientations[boneNumbers[1]], aSkinnedNormal) * boneWeights[1] +
			ApplyQuaternion(uBoneOrientations[boneNumbers[2]], aSkinnedNormal) * boneWeights[2] +
			ApplyQuaternion(uBoneOrientations[boneNumbers[3]], aSkinnedNormal) * boneWeights[3];
	}
	else
	{
		Value<mat4x4> tmpWorld = key.instanced ? uWorlds[instancer->GetInstanceID()] : uWorld;

		tmpVertexPosition = mul(tmpWorld, newvec4(aPosition, 1.0f));
		tmpVertexNormal = mul(tmpWorld.Cast<mat3x3>(), aNormal);
	}
}

void Painter::BeginMaterialLighting(const PixelShaderKey& key, Value<vec3> ambientColor)
{
	tmpWorldPosition = newvec4(iWorldPosition, 1.0f);
	tmpTexcoord = iTexcoord;

	// получить нормаль
	if(key.materialKey.hasNormalTexture)
	{
		Value<vec3> dxPosition = ddx(iWorldPosition);
		Value<vec3> dyPosition = ddy(iWorldPosition);
		Value<vec2> dxTexcoord = ddx(tmpTexcoord);
		Value<vec2> dyTexcoord = ddy(tmpTexcoord);

		Value<vec3> r0 = cross(dxPosition, dyPosition);

		Value<vec3> r1 = cross(dyPosition, r0);
		Value<vec3> r2 = cross(r0, dxPosition);

		Value<vec3> T1 = normalize(r1 * dxTexcoord["x"] + r2 * dyTexcoord["x"]);
		Value<vec3> T2 = normalize(r1 * dxTexcoord["y"] + r2 * dyTexcoord["y"]);
		Value<vec3> T3 = normalize(iNormal);

		Value<vec3> perturbedNormal = (uNormalSampler.Sample(tmpTexcoord * uNormalCoordTransform["xy"] + uNormalCoordTransform["zw"]) * Value<float>(2) - Value<float>(1));

		tmpNormal = normalize(T1 * perturbedNormal["x"] + T2 * perturbedNormal["y"] + T3 * perturbedNormal["z"]);
	}
	else
		tmpNormal = normalize(iNormal);

	tmpToCamera = normalize(uCameraPosition - iWorldPosition);
	tmpDiffuse = key.materialKey.hasDiffuseTexture ? uDiffuseSampler.Sample(tmpTexcoord) : uDiffuse;
	tmpSpecular = key.materialKey.hasSpecularTexture ? uSpecularSampler.Sample(tmpTexcoord) : uSpecular;
	tmpSpecularExponent = exp2(tmpSpecular["x"] * val(4.0f/*12.0f*/));
	tmpColor = ambientColor * tmpDiffuse["xyz"];
}

void Painter::ApplyMaterialLighting(Value<vec3> lightPosition, Value<vec3> lightColor)
{
	// направление на свет
	Value<vec3> tmpToLight = normalize(lightPosition - iWorldPosition);
	// биссектриса между направлениями на свет и камеру
	Value<vec3> tmpLightViewBissect = normalize(tmpToLight + tmpToCamera);
	// диффузная составляющая
	Value<vec3> tmpDiffusePart = tmpDiffuse["xyz"]
		* max(dot(tmpNormal, tmpToLight), Value<float>(0.0f));
	// specular составляющая
	Value<vec3> tmpSpecularPart = tmpDiffuse["xyz"]
		* pow(max(dot(tmpLightViewBissect, tmpNormal), Value<float>(0.0f)), tmpSpecularExponent)
		//* dot(tmpNormal, tmpToLight) // хз, может не нужно оно?
		* (tmpSpecularExponent + Value<float>(1)) / (max(pow(dot(tmpToLight, tmpLightViewBissect), val(3.0f)), val(0.1f)) * val(8.0f));

	// результирующая добавка к цвету
	tmpColor += lightColor * (tmpDiffusePart + tmpSpecularPart);
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

	GetWorldPositionAndNormal(key);

	Expression e = (
		setPosition(mul(uViewProj, tmpVertexPosition)),
		iNormal.Set(tmpVertexNormal),
		iTexcoord.Set(key.skinned ? aSkinnedTexcoord : aTexcoord),
		iWorldPosition.Set(tmpVertexPosition["xyz"])
	);

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

	GetWorldPositionAndNormal(key);

	Value<vec4> p = mul(uViewProj, tmpVertexPosition);

	ptr<VertexShader> vertexShader = shaderCache->GetVertexShader(Expression((
		setPosition(p),
		iDepth.Set(p["z"])
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
	BeginMaterialLighting(key, lightVariant.uAmbientColor);

	// учесть все простые источники света
	for(int i = 0; i < basicLightsCount; ++i)
	{
		BasicLight& basicLight = lightVariant.basicLights[i];

		ApplyMaterialLighting(basicLight.uLightPosition, basicLight.uLightColor);
	}

	// учесть все источники света с тенями
	for(int i = 0; i < shadowLightsCount; ++i)
	{
		ShadowLight& shadowLight = lightVariant.shadowLights[i];

		// тень
		Value<vec4> shadowCoords = mul(shadowLight.uLightTransform, tmpWorldPosition);
		Value<float> lighted = (shadowCoords["z"] > val(0.0f)).Cast<float>();
		Value<float> linearShadowZ = shadowCoords["z"];
		//lighted = lighted * (linearShadowZ > Value<float>(0));
		shadowCoords = shadowCoords / shadowCoords["w"];
		lighted = lighted * (abs(shadowCoords["x"]) < val(1.0f)).Cast<float>() * (abs(shadowCoords["y"]) < val(1.0f)).Cast<float>();
		Value<vec2> shadowCoordsXY = screenToTexture(shadowCoords["xy"]);
		Value<float> shadowMultiplier = lighted * saturate(exp(val(4.0f) * (shadowLight.uShadowSampler.Sample(shadowCoordsXY) - linearShadowZ)));

		ApplyMaterialLighting(shadowLight.uLightPosition, shadowLight.uLightColor * shadowMultiplier);
	}

	ptr<PixelShader> pixelShader = shaderCache->GetPixelShader((
		iNormal,
		iTexcoord,
		iWorldPosition,
		fragment(0, newvec4(tmpColor, tmpDiffuse["w"]))
	));

	// добавить и вернуть
	pixelShaderCache.insert(std::make_pair(key, pixelShader));
	return pixelShaderCache.find(key)->second;
}

void Painter::BeginFrame(float frameTime)
{
	this->frameTime = frameTime;

	models.clear();
	skinnedModels.clear();
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
			uViewProj.Set(lights[i].transform);
			ugCamera->Upload(context);

			// очистить карту теней
			context->ClearColor(0, vec4(1e8, 1e8, 1e8, 1e8));
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
				Context::LetVertexShader lvs(context, GetVertexShadowShader(VertexShaderKey(true, false)));
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
						uWorlds.Set(k, models[j + k].worldTransform);
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
				Context::LetVertexShader lvs(context, GetVertexShadowShader(VertexShaderKey(false, true)));
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
						uBoneOrientations.Set(k, orientations[k]);
						uBoneOffsets.Set(k, vec4(offsets[k].x, offsets[k].y, offsets[k].z, 0));
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
					Context::LetSampler ls(context, uShadowBlurSourceSampler, rb->GetTexture(), ssPoint);
					Context::LetUniformBuffer lub(context, ugShadowBlur);

					uShadowBlurDirection.Set(vec2(1.0f / shadowMapSize, 0));
					ugShadowBlur->Upload(context);

					context->ClearColor(0, vec4(0, 0, 0, 0));
					context->Draw();
				}

				// второй проход
				{
					Context::LetFrameBuffer lfb(context, fbShadowBlurs[i]);
					Context::LetSampler ls(context, uShadowBlurSourceSampler, rbShadowBlur->GetTexture(), ssPoint);
					Context::LetUniformBuffer lub(context, ugShadowBlur);

					uShadowBlurDirection.Set(vec2(0, 1.0f / shadowMapSize));
					ugShadowBlur->Upload(context);

					context->ClearColor(0, vec4(0, 0, 0, 0));
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
	};

	{
		Context::LetFrameBuffer lfb(context, fbOpaque);
		Context::LetViewport lv(context, screenWidth, screenHeight);
		Context::LetDepthTestFunc ldtf(context, Context::depthTestFuncLess);
		Context::LetDepthWrite ldw(context, true);
		Context::LetUniformBuffer lubCamera(context, ugCamera);

		// установить uniform'ы камеры
		uViewProj.Set(cameraViewProj);
		uInvViewProj.Set(cameraInvViewProj);
		uCameraPosition.Set(cameraPosition);
		ugCamera->Upload(context);

		// установить параметры источников света
		LightVariant& lightVariant = GetLightVariant(LightVariantKey(basicLightsCount, shadowLightsCount));
		Context::LetUniformBuffer lubLight(context, lightVariant.ugLight);

		lightVariant.uAmbientColor.Set(ambientColor);
		int basicLightNumber = 0;
		int shadowLightNumber = 0;
		Context::LetSampler ls[maxShadowLightsCount];
		for(size_t i = 0; i < lights.size(); ++i)
			if(lights[i].shadow)
			{
				ShadowLight& shadowLight = lightVariant.shadowLights[shadowLightNumber];
				shadowLight.uLightPosition.Set(lights[i].position);
				shadowLight.uLightColor.Set(lights[i].color);
				shadowLight.uLightTransform.Set(lights[i].transform);

				ls[shadowLightNumber](context, shadowLight.uShadowSampler, rbShadows[shadowLightNumber]->GetTexture(), shadowSamplerState);

				shadowLightNumber++;
			}
			else
			{
				BasicLight& basicLight = lightVariant.basicLights[basicLightNumber++];
				basicLight.uLightPosition.Set(lights[i].position);
				basicLight.uLightColor.Set(lights[i].color);
			}
		lightVariant.ugLight->Upload(context);

		// очистить рендербуферы
		context->ClearColor(0, vec4(0, 0, 0, 1)); // color
		context->ClearColor(1, vec4(0, 0, 0, 1)); // normal
		context->ClearDepth(1.0f);

		//** нарисовать простые модели
		{
			std::sort(models.begin(), models.end(), Sorter());

			// установить привязку атрибутов
			Context::LetAttributeBinding lab(context, abInstanced);
			// установить вершинный шейдер
			Context::LetVertexShader lvs(context, GetVertexShader(VertexShaderKey(true, false)));
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
				uDiffuse.Set(material->diffuse);
				uSpecular.Set(material->specular);
				uNormalCoordTransform.Set(material->normalCoordTransform);
				ugMaterial->Upload(context);

				// рисуем инстансингом обычные модели
				// установить пиксельный шейдер
				Context::LetPixelShader lps(context, GetPixelShader(PixelShaderKey(basicLightsCount, shadowLightsCount, material->GetKey())));
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
						uWorlds.Set(k, models[i + j + k].worldTransform);
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
			Context::LetVertexShader lvs(context, GetVertexShader(VertexShaderKey(false, true)));
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
				uDiffuse.Set(material->diffuse);
				uSpecular.Set(material->specular);
				uNormalCoordTransform.Set(material->normalCoordTransform);
				ugMaterial->Upload(context);

				// установить пиксельный шейдер
				Context::LetPixelShader lps(context, GetPixelShader(PixelShaderKey(basicLightsCount, shadowLightsCount, material->GetKey())));

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
					uBoneOrientations.Set(k, orientations[k]);
					uBoneOffsets.Set(k, vec4(offsets[k].x, offsets[k].y, offsets[k].z, 0));
				}
				ugSkinnedModel->Upload(context);

				// нарисовать
				context->Draw();
			}
		}
	}

	// всё, теперь постпроцессинг
	{
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
		uDownsampleBlend.Set(1.0f - exp(frameTime * (-0.79f)));
		for(int i = 0; i < downsamplingPassesCount; ++i)
		{
			float halfSourcePixelWidth = 0.5f / (i == 0 ? screenWidth : (1 << (downsamplingPassesCount - i)));
			float halfSourcePixelHeight = 0.5f / (i == 0 ? screenHeight : (1 << (downsamplingPassesCount - i)));
			uDownsampleOffsets.Set(vec4(-halfSourcePixelWidth, halfSourcePixelWidth, -halfSourcePixelHeight, halfSourcePixelHeight));
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
				context->ClearColor(0, vec4(0, 0, 0, 0));
			context->Draw();
		}
		veryFirstDownsampling = false;

		// bloom
		{
			uBloomLimit.Set(bloomLimit);
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
					context->ClearColor(0, vec4(0, 0, 0, 0));
					context->Draw();
				}
				{
					Context::LetFrameBuffer lfb(context, fbBloom1);
					Context::LetSampler ls(context, uBloomSourceSampler, rbBloom2->GetTexture(), ssLinear);
					Context::LetPixelShader lps(context, psBloom2);
					context->ClearColor(0, vec4(0, 0, 0, 0));
					context->Draw();
				}
				for(int i = 1; i < bloomPassesCount; ++i)
				{
					{
						Context::LetFrameBuffer lfb(context, fbBloom2);
						Context::LetSampler ls(context, uBloomSourceSampler, rbBloom1->GetTexture(), ssLinear);
						Context::LetPixelShader lps(context, psBloom1);
						context->ClearColor(0, vec4(0, 0, 0, 0));
						context->Draw();
					}
					{
						Context::LetFrameBuffer lfb(context, fbBloom1);
						Context::LetSampler ls(context, uBloomSourceSampler, rbBloom2->GetTexture(), ssLinear);
						Context::LetPixelShader lps(context, psBloom2);
						context->ClearColor(0, vec4(0, 0, 0, 0));
						context->Draw();
					}
				}
			}
			else
			{
				Context::LetFrameBuffer lfb(context, fbBloom1);
				Context::LetSampler ls(context, uBloomSourceSampler, rbBloom2->GetTexture(), ssLinear);
				Context::LetPixelShader lps(context, psBloom2);
				context->ClearColor(0, vec4(0, 0, 0, 0));
			}
		}

		// tone mapping
		{
			Context::LetFrameBuffer lfb(context, presenter->GetFrameBuffer());
			Context::LetViewport lv(context, screenWidth, screenHeight);
			Context::LetSampler lsBloom(context, uToneBloomSampler, rbBloom1->GetTexture(), ssLinear);
			Context::LetSampler lsScreen(context, uToneScreenSampler, rbScreen->GetTexture(), ssPoint);
			Context::LetSampler lsAverage(context, uToneAverageSampler, rbDownsamples[downsamplingPassesCount - 1]->GetTexture(), ssPoint);

			uToneLuminanceKey.Set(toneLuminanceKey);
			uToneMaxLuminance.Set(toneMaxLuminance);
			ugTone->Upload(context);
			Context::LetUniformBuffer lub(context, ugTone);

			Context::LetPixelShader lps(context, psTone);

			context->ClearColor(0, vec4(0, 0, 0, 0));
			context->Draw();
		}
	} // postprocessing
}
