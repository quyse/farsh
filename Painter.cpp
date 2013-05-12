#include "Painter.hpp"
#include "BoneAnimation.hpp"
#include "GeometryFormats.hpp"

const int Painter::shadowMapSize = 1024;
const int Painter::randomMapSize = 64;
const int Painter::downsamplingStepForBloom = 1;
const int Painter::bloomMapSize = 1 << (Painter::downsamplingPassesCount - 1 - Painter::downsamplingStepForBloom);

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

//*** Painter::VertexShaderKey

Painter::VertexShaderKey::VertexShaderKey(bool instanced, bool skinned, bool decal)
: instanced(instanced), skinned(skinned), decal(decal) {}

Painter::VertexShaderKey::operator size_t() const
{
	return (size_t)instanced | ((size_t)skinned << 1) | ((size_t)decal << 2);
}

//*** Painter::PixelShaderKey

Painter::PixelShaderKey::PixelShaderKey(int basicLightsCount, int shadowLightsCount, bool decal, const MaterialKey& materialKey) :
basicLightsCount(basicLightsCount), shadowLightsCount(shadowLightsCount), decal(decal), materialKey(materialKey)
{}

Painter::PixelShaderKey::operator size_t() const
{
	return basicLightsCount | (shadowLightsCount << 3) | ((size_t)decal << 6) | (((size_t)materialKey) << 7);
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
	geometryFormats(geometryFormats),
	screenWidth(screenWidth),
	screenHeight(screenHeight),
	shaderCache(shaderCache),

	ab(device->CreateAttributeBinding(geometryFormats->al)),
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

	ugModel(NEW(UniformGroup(3))),
	uWorld(ugModel->AddUniform<mat4x4>()),

	ugInstancedModel(NEW(UniformGroup(3))),
	uWorlds(ugInstancedModel->AddUniformArray<mat4x4>(maxInstancesCount)),

	ugSkinnedModel(NEW(UniformGroup(3))),
	uBoneOffsets(ugSkinnedModel->AddUniformArray<vec4>(maxBonesCount)),
	uBoneOrientations(ugSkinnedModel->AddUniformArray<vec4>(maxBonesCount)),

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

	fTarget(0),
	fNormal(1),

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
	rbBack = presenter->GetBackBuffer();
	dsbDepth = device->CreateDepthStencilBuffer(screenWidth, screenHeight, true);
	dsbShadow = device->CreateDepthStencilBuffer(shadowMapSize, shadowMapSize, false);
	for(int i = 0; i < maxShadowLightsCount; ++i)
		rbShadows[i] = device->CreateRenderBuffer(shadowMapSize, shadowMapSize, PixelFormats::floatR16);
	rbShadowBlur = device->CreateRenderBuffer(shadowMapSize, shadowMapSize, PixelFormats::floatR16);

	// экранный буфер
	rbScreen = device->CreateRenderBuffer(screenWidth, screenHeight, PixelFormats::floatR11G11B10);
	// экранный буфер нормалей
	rbScreenNormal = device->CreateRenderBuffer(screenWidth, screenHeight, PixelFormats::floatR11G11B10);
	// буферы для downsample
	for(int i = 0; i < downsamplingPassesCount; ++i)
		rbDownsamples[i] = device->CreateRenderBuffer(1 << (downsamplingPassesCount - 1 - i), 1 << (downsamplingPassesCount - 1 - i),
			i <= downsamplingStepForBloom ? PixelFormats::floatR11G11B10 : PixelFormats::floatR16);
	// буферы для Bloom
	rbBloom1 = device->CreateRenderBuffer(bloomMapSize, bloomMapSize, PixelFormats::floatR11G11B10);
	rbBloom2 = device->CreateRenderBuffer(bloomMapSize, bloomMapSize, PixelFormats::floatR11G11B10);

	shadowSamplerState = device->CreateSamplerState();
	shadowSamplerState->SetWrap(SamplerState::wrapBorder, SamplerState::wrapBorder, SamplerState::wrapBorder);
	shadowSamplerState->SetFilter(SamplerState::filterLinear, SamplerState::filterLinear, SamplerState::filterLinear);
	{
		float borderColor[] = { 0, 0, 0, 0 };
		shadowSamplerState->SetBorderColor(borderColor);
	}

	uScreenNormalSampler.SetTexture(rbScreenNormal->GetTexture());
	uScreenNormalSampler.SetSamplerState(shadowSamplerState);
	uScreenDepthSampler.SetTexture(dsbDepth->GetTexture());
	uScreenDepthSampler.SetSamplerState(shadowSamplerState);

	// создать случайную текстуру
	if(0)
	{
		int width = randomMapSize, height = randomMapSize;
		ptr<File> randomTextureFile = NEW(MemoryFile(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + width * height * 4));
		BITMAPFILEHEADER* bfh = (BITMAPFILEHEADER*)randomTextureFile->GetData();
		ZeroMemory(bfh, sizeof(*bfh));
		bfh->bfType = 'MB';
		bfh->bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + width * height * 4;
		bfh->bfReserved1 = 0;
		bfh->bfReserved2 = 0;
		bfh->bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)(bfh + 1);
		ZeroMemory(bih, sizeof(*bih));
		bih->biSize = sizeof(BITMAPINFOHEADER);
		bih->biWidth = width;
		bih->biHeight = height;
		bih->biPlanes = 1;
		bih->biBitCount = 32;
		unsigned char* pixels = (unsigned char*)(bih + 1);
		int count = width * height * 4;
		for(int i = 0; i < count; ++i)
			pixels[i] = rand() % 256;
		randomTexture = device->CreateStaticTexture(randomTextureFile);
		//uRandomSampler.SetTexture(randomTexture);
		ptr<SamplerState> ss = device->CreateSamplerState();
		ss->SetWrap(SamplerState::wrapRepeat, SamplerState::wrapRepeat, SamplerState::wrapRepeat);
		//uRandomSampler.SetSamplerState(ss);
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
#ifdef FARSH_USE_OPENGL
			unsigned short indices[] = { 0, 1, 2, 0, 2, 3 };
#else
			unsigned short indices[] = { 0, 2, 1, 0, 3, 2 };
#endif

			vb = device->CreateStaticVertexBuffer(MemoryFile::CreateViaCopy(vertices, sizeof(vertices)), vl);
			ib = device->CreateStaticIndexBuffer(MemoryFile::CreateViaCopy(indices, sizeof(indices)), sizeof(unsigned short));

			ab = device->CreateAttributeBinding(al);
		}
	} quad(device);

	//** инициализировать состояния конвейера

	// shadow pass
	csShadow.viewportWidth = shadowMapSize;
	csShadow.viewportHeight = shadowMapSize;
	csShadow.depthStencilBuffer = dsbShadow;
	ugCamera->Apply(csShadow);

	// пиксельный шейдер для теней
	csShadow.pixelShader = shaderCache->GetPixelShader(Expression((
		iDepth,
		fTarget = newvec4(iDepth, 0, 0, 0)
		)));

	//** шейдеры и состояния постпроцессинга и размытия теней
	{
		ContextState csFilter;
		csFilter.viewportWidth = screenWidth;
		csFilter.viewportHeight = screenHeight;
		csFilter.attributeBinding = quad.ab;
		csFilter.vertexBuffers[0] = quad.vb;
		csFilter.indexBuffer = quad.ib;
		csFilter.depthTestFunc = ContextState::depthTestFuncAlways;
		csFilter.depthWrite = false;

		// промежуточные
		Interpolant<vec2> iTexcoord(0);
		// результат
		Fragment<vec4> fTarget(0);

		// вершинный шейдер - общий для всех постпроцессингов
		ptr<VertexShader> vertexShader = shaderCache->GetVertexShader((
			setPosition(quad.aPosition),
			iTexcoord = quad.aTexcoord
			));

		csFilter.vertexShader = vertexShader;

		// пиксельный шейдер для размытия тени
		ptr<PixelShader> psShadowBlur;
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
				fTarget = newvec4(log(sum), 0, 0, 1)
				));
			psShadowBlur = shaderCache->GetPixelShader(shader);
		}

		// пиксельный шейдер для downsample
		ptr<PixelShader> psDownsample;
		{
			Expression shader = (
				iTexcoord,
				fTarget = newvec4((
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xz"]) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xw"]) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yz"]) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yw"])
					) * Value<float>(0.25f), 1.0f)
				);
			psDownsample = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для первого downsample luminance
		ptr<PixelShader> psDownsampleLuminanceFirst;
		{
			Temp<vec3> luminanceCoef;
			Expression shader = (
				iTexcoord,
				luminanceCoef = newvec3(0.2126f, 0.7152f, 0.0722f),
				fTarget = newvec4((
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xz"]), luminanceCoef) + Value<float>(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xw"]), luminanceCoef) + Value<float>(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yz"]), luminanceCoef) + Value<float>(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yw"]), luminanceCoef) + Value<float>(0.0001f))
					) * Value<float>(0.25f), 0.0f, 0.0f, 1.0f)
				);
			psDownsampleLuminanceFirst = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для downsample luminance
		ptr<PixelShader> psDownsampleLuminance;
		{
			Expression shader = (
				iTexcoord,
				fTarget = newvec4((
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xz"]) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["xw"]) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yz"]) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets["yw"])
					) * Value<float>(0.25f), 0.0f, 0.0f, uDownsampleBlend)
				);
			psDownsampleLuminance = shaderCache->GetPixelShader(shader);
		}

		// точки для шейдера
		//const float offsets[] = { -7, -3, -1, 0, 1, 3, 7 };
		const float offsets[] = { -7, -5.9f, -3.2f, -2.1f, -1.1f, -0.5f, 0, 0.5f, 1.1f, 2.1f, 3.2f, 5.9f, 7 };
		const float offsetScaleX = 1.0f / bloomMapSize, offsetScaleY = 1.0f / bloomMapSize;
		// пиксельный шейдер для самого первого прохода (с ограничением по освещённости)
		ptr<PixelShader> psBloomLimit;
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
				fTarget = newvec4(sum * Value<float>(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f)
				));
			psBloomLimit = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для первого прохода
		ptr<PixelShader> psBloom1;
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
				fTarget = newvec4(sum * Value<float>(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f)
				));
			psBloom1 = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для второго прохода
		ptr<PixelShader> psBloom2;
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
				fTarget = newvec4(sum * Value<float>(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f)
				));
			psBloom2 = shaderCache->GetPixelShader(shader);
		}
		// шейдер tone mapping
		ptr<PixelShader> psTone;
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
				fTarget = newvec4(color, 1.0f)
			);
			psTone = shaderCache->GetPixelShader(shader);
		}

		csShadowBlur = csFilter;
		csBloomLimit = csFilter;
		csBloom1 = csFilter;
		csBloom2 = csFilter;
		csTone = csFilter;

		// point sampler
		ptr<SamplerState> pointSampler = device->CreateSamplerState();
		pointSampler->SetFilter(SamplerState::filterPoint, SamplerState::filterPoint, SamplerState::filterPoint);
		pointSampler->SetWrap(SamplerState::wrapClamp, SamplerState::wrapClamp, SamplerState::wrapClamp);
		// linear sampler
		ptr<SamplerState> linearSampler = device->CreateSamplerState();
		linearSampler->SetFilter(SamplerState::filterLinear, SamplerState::filterLinear, SamplerState::filterLinear);
		linearSampler->SetWrap(SamplerState::wrapClamp, SamplerState::wrapClamp, SamplerState::wrapClamp);
		// point sampler with border=0
		ptr<SamplerState> pointBorderSampler = device->CreateSamplerState();
		pointBorderSampler->SetFilter(SamplerState::filterPoint, SamplerState::filterPoint, SamplerState::filterPoint);
		pointBorderSampler->SetWrap(SamplerState::wrapBorder, SamplerState::wrapBorder, SamplerState::wrapBorder);
		float borderColor[] = { 0, 0, 0, 0 };
		pointBorderSampler->SetBorderColor(borderColor);

		// состояние для размытия тени
		csShadowBlur.viewportWidth = shadowMapSize;
		csShadowBlur.viewportHeight = shadowMapSize;
		uShadowBlurSourceSampler.SetSamplerState(pointBorderSampler);
		uShadowBlurSourceSampler.Apply(csShadowBlur);
		ugShadowBlur->Apply(csShadowBlur);
		csShadowBlur.pixelShader = psShadowBlur;

		// проходы даунсемплинга
		for(int i = 0; i < downsamplingPassesCount; ++i)
		{
			ContextState& cs = csDownsamples[i];
			cs = csFilter;

			cs.renderBuffers[0] = rbDownsamples[i];
			cs.viewportWidth = 1 << (downsamplingPassesCount - 1 - i);
			cs.viewportHeight = 1 << (downsamplingPassesCount - 1 - i);
			ugDownsample->Apply(cs);
			if(i <= downsamplingStepForBloom + 1)
			{
				uDownsampleSourceSampler.SetTexture(i == 0 ? rbScreen->GetTexture() : rbDownsamples[i - 1]->GetTexture());
				uDownsampleSourceSampler.SetSamplerState(i == 0 ? linearSampler : pointSampler);
				uDownsampleSourceSampler.Apply(cs);
			}
			else
			{
				uDownsampleLuminanceSourceSampler.SetTexture(rbDownsamples[i - 1]->GetTexture());
				uDownsampleLuminanceSourceSampler.SetSamplerState(pointSampler);
				uDownsampleLuminanceSourceSampler.Apply(cs);
			}

			if(i <= downsamplingStepForBloom)
				cs.pixelShader = psDownsample;
			else if(i == downsamplingStepForBloom + 1)
				cs.pixelShader = psDownsampleLuminanceFirst;
			else
				cs.pixelShader = psDownsampleLuminance;
		}
		// для последнего прохода - специальный blend state
		{
			ptr<BlendState> bs = device->CreateBlendState();
			bs->SetColor(BlendState::colorSourceSrcAlpha, BlendState::colorSourceInvSrcAlpha, BlendState::operationAdd);
			csDownsamples[downsamplingPassesCount - 1].blendState = bs;
		}

		// самый первый проход bloom (из rbDownsamples[downsamplingStepForBloom] в rbBloom2)
		csBloomLimit.viewportWidth = bloomMapSize;
		csBloomLimit.viewportHeight = bloomMapSize;
		csBloomLimit.renderBuffers[0] = rbBloom2;
		uBloomSourceSampler.SetTexture(rbDownsamples[downsamplingStepForBloom]->GetTexture());
		uBloomSourceSampler.SetSamplerState(linearSampler);
		uBloomSourceSampler.Apply(csBloomLimit);
		ugBloom->Apply(csBloomLimit);
		csBloomLimit.pixelShader = psBloomLimit;
		// первый проход bloom (из rbBloom1 в rbBloom2)
		csBloom1.viewportWidth = bloomMapSize;
		csBloom1.viewportHeight = bloomMapSize;
		csBloom1.renderBuffers[0] = rbBloom2;
		uBloomSourceSampler.SetTexture(rbBloom1->GetTexture());
		uBloomSourceSampler.SetSamplerState(linearSampler);
		uBloomSourceSampler.Apply(csBloom1);
		ugBloom->Apply(csBloom1);
		csBloom1.pixelShader = psBloom1;
		// второй проход bloom (из rbBloom2 в rbBloom1)
		csBloom2.viewportWidth = bloomMapSize;
		csBloom2.viewportHeight = bloomMapSize;
		csBloom2.renderBuffers[0] = rbBloom1;
		uBloomSourceSampler.SetTexture(rbBloom2->GetTexture());
		uBloomSourceSampler.SetSamplerState(linearSampler);
		uBloomSourceSampler.Apply(csBloom2);
		ugBloom->Apply(csBloom2);
		csBloom2.pixelShader = psBloom2;
		// tone mapping
		csTone.viewportWidth = screenWidth;
		csTone.viewportHeight = screenHeight;
		csTone.renderBuffers[0] = rbBack;
		uToneBloomSampler.SetTexture(rbBloom1->GetTexture());
		uToneBloomSampler.SetSamplerState(linearSampler);
		uToneBloomSampler.Apply(csTone);
		uToneScreenSampler.SetTexture(rbScreen->GetTexture());
		uToneScreenSampler.SetSamplerState(pointSampler);
		uToneScreenSampler.Apply(csTone);
		uToneAverageSampler.SetTexture(rbDownsamples[downsamplingPassesCount - 1]->GetTexture());
		uToneAverageSampler.SetSamplerState(pointSampler);
		uToneAverageSampler.Apply(csTone);
		ugTone->Apply(csTone);
		csTone.pixelShader = psTone;
	}
}

Painter::LightVariant& Painter::GetLightVariant(const LightVariantKey& key)
{
	// если он уже есть в кэше, вернуть
	{
		std::unordered_map<LightVariantKey, LightVariant>::iterator i = lightVariantsCache.find(key);
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

	// инициализировать состояние контекста
	ContextState& cs = lightVariant.csOpaque;
	cs.viewportWidth = screenWidth;
	cs.viewportHeight = screenHeight;
	cs.renderBuffers[0] = rbScreen;
	cs.renderBuffers[1] = rbScreenNormal;
	cs.depthStencilBuffer = dsbDepth;
	cs.depthTestFunc = ContextState::depthTestFuncLess;
	cs.depthWrite = true;
	ugCamera->Apply(cs);
	lightVariant.ugLight->Apply(cs);
	ugMaterial->Apply(cs);

	// применить семплеры карт теней
	for(int i = 0; i < shadowLightsCount; ++i)
	{
		ShadowLight& shadowLight = lightVariant.shadowLights[i];
		shadowLight.uShadowSampler.SetTexture(rbShadows[i]->GetTexture());
		shadowLight.uShadowSampler.SetSamplerState(shadowSamplerState);
		shadowLight.uShadowSampler.Apply(cs);
	}

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
				tmpInstance = getInstanceID(),
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
		std::unordered_map<VertexShaderKey, ptr<VertexShader> >::iterator i = vertexShaderCache.find(key);
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
		std::unordered_map<VertexShaderKey, ptr<VertexShader> >::iterator i = vertexShadowShaderCache.find(key);
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
		std::unordered_map<PixelShaderKey, ptr<PixelShader> >::iterator i = pixelShaderCache.find(key);
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
			shadowCoordsXY = newvec2(
				(shadowCoords["x"] + Value<float>(1.0f)) * Value<float>(0.5f),
				(Value<float>(1.0f) - shadowCoords["y"]) * Value<float>(0.5f)),
			shadowMultiplier = lighted * saturate(exp(Value<float>(4) * (shadowLight.uShadowSampler.Sample(shadowCoordsXY) - linearShadowZ))),
			
			ApplyMaterialLighting(shadowLight.uLightPosition, shadowLight.uLightColor * shadowMultiplier)
			));
	}

	// вернуть цвет
	shader.Append((
		fTarget = newvec4(tmpColor, tmpDiffuse["w"])
	));
	// если не декали, вернуть нормаль
	if(!key.decal)
		shader.Append((
			fNormal = newvec4((tmpNormal + Value<float>(1)) * Value<float>(0.5f), 1)
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
			ContextState& cs = context->GetTargetState();

			cs = csShadow;

			ptr<RenderBuffer> rb = rbShadows[shadowPassNumber];
			cs.renderBuffers[0] = rb;

			// указать трансформацию
			uViewProj.SetValue(lights[i].transform);
			ugCamera->Upload(context);

			// очистить карту теней
			context->ClearDepthStencilBuffer(dsbShadow, 1.0f);
			context->ClearRenderBuffer(rb, farColor);

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

			// установить привязку атрибутов
			cs.attributeBinding = ab;
			// установить вершинный шейдер
			cs.vertexShader = GetVertexShadowShader(VertexShaderKey(true, false, false));
			// установить константный буфер
			ugInstancedModel->Apply(cs);

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
				cs.vertexBuffers[0] = models[j].geometry->GetVertexBuffer();
				cs.indexBuffer = models[j].geometry->GetIndexBuffer();
				// установить uniform'ы
				for(int k = 0; k < batchCount; ++k)
					uWorlds.SetValue(k, models[j + k].worldTransform);
				// и залить в GPU
				ugInstancedModel->Upload(context);

				// нарисовать
				context->DrawInstanced(batchCount);

				j += batchCount;
			}

			//** рисуем skinned-модели

			// отсортировать объекты по геометрии
			std::sort(skinnedModels.begin(), skinnedModels.end(), GeometrySorter());

			// установить привязку атрибутов
			cs.attributeBinding = abSkinned;
			// установить вершинный шейдер
			cs.vertexShader = GetVertexShadowShader(VertexShaderKey(false, true, false));
			// установить константный буфер
			ugSkinnedModel->Apply(cs);

			// нарисовать с группировкой по геометрии
			ptr<Geometry> lastGeometry;
			for(size_t j = 0; j < skinnedModels.size(); ++j)
			{
				const SkinnedModel& skinnedModel = skinnedModels[j];
				// установить геометрию, если отличается
				if(lastGeometry != skinnedModel.shadowGeometry)
				{
					cs.vertexBuffers[0] = skinnedModel.shadowGeometry->GetVertexBuffer();
					cs.indexBuffer = skinnedModel.shadowGeometry->GetIndexBuffer();
					lastGeometry = skinnedModel.shadowGeometry;
				}
				// установить uniform'ы костей
				ptr<BoneAnimationFrame> animationFrame = skinnedModel.animationFrame;
				const std::vector<quat>& orientations = animationFrame->orientations;
				const std::vector<vec3>& offsets = animationFrame->offsets;
				int bonesCount = (int)orientations.size();
#ifdef _DEBUG
				if(bonesCount > maxBonesCount)
					THROW_PRIMARY_EXCEPTION("Too many bones");
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

			// выполнить размытие тени
			// первый проход
			cs = csShadowBlur;
			cs.renderBuffers[0] = rbShadowBlur;
			uShadowBlurSourceSampler.SetTexture(rb->GetTexture());
			uShadowBlurSourceSampler.Apply(cs);
			uShadowBlurDirection.SetValue(vec2(1.0f / shadowMapSize, 0));
			ugShadowBlur->Upload(context);
			context->ClearRenderBuffer(rbShadowBlur, zeroColor);
			context->Draw();
			// второй проход
			cs = csShadowBlur;
			cs.renderBuffers[0] = rb;
			uShadowBlurSourceSampler.SetTexture(rbShadowBlur->GetTexture());
			uShadowBlurSourceSampler.Apply(cs);
			uShadowBlurDirection.SetValue(vec2(0, 1.0f / shadowMapSize));
			ugShadowBlur->Upload(context);
			context->ClearRenderBuffer(rb, zeroColor);
			context->Draw();

			shadowPassNumber++;
		}

	// очистить рендербуферы
	float color[4] = { 0, 0, 0, 1 };
	float colorDepth[4] = { 1, 1, 1, 1 };
	context->ClearRenderBuffer(rbScreen, color);
	context->ClearRenderBuffer(rbScreenNormal, color);
	context->ClearDepthStencilBuffer(dsbDepth, 1.0f);

	ContextState& cs = context->GetTargetState();

	// установить uniform'ы камеры
	uViewProj.SetValue(cameraViewProj);
	uInvViewProj.SetValue(cameraInvViewProj);
	uCameraPosition.SetValue(cameraPosition);
	ugCamera->Upload(context);

	// установить параметры источников света
	LightVariant& lightVariant = GetLightVariant(LightVariantKey(basicLightsCount, shadowLightsCount));
	cs = lightVariant.csOpaque;
	lightVariant.uAmbientColor.SetValue(ambientColor);
	int basicLightNumber = 0;
	int shadowLightNumber = 0;
	for(size_t i = 0; i < lights.size(); ++i)
		if(lights[i].shadow)
		{
			ShadowLight& shadowLight = lightVariant.shadowLights[shadowLightNumber++];
			shadowLight.uLightPosition.SetValue(lights[i].position);
			shadowLight.uLightColor.SetValue(lights[i].color);
			shadowLight.uLightTransform.SetValue(lights[i].transform);
		}
		else
		{
			BasicLight& basicLight = lightVariant.basicLights[basicLightNumber++];
			basicLight.uLightPosition.SetValue(lights[i].position);
			basicLight.uLightColor.SetValue(lights[i].color);
		}
	lightVariant.ugLight->Upload(context);

	// сортировщик моделей по материалу, а затем по геометрии
	struct Sorter
	{
		bool operator()(const Model& a, const Model& b) const
		{
			return a.material < b.material || a.material == b.material && a.geometry < b.geometry;
		}
		bool operator()(const SkinnedModel& a, const SkinnedModel& b) const
		{
			return a.material < b.material || a.material == b.material && a.geometry < b.geometry;
		}
		// для декалей - только по материалу
		bool operator()(const Decal& a, const Decal& b) const
		{
			return a.material < b.material;
		}
	};

	//** нарисовать простые модели

	std::sort(models.begin(), models.end(), Sorter());

	// установить привязку атрибутов
	cs.attributeBinding = ab;
	// установить вершинный шейдер
	cs.vertexShader = GetVertexShader(VertexShaderKey(true, false, false));
	// установить константный буфер
	ugInstancedModel->Apply(cs);

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
		uDiffuseSampler.SetTexture(material->diffuseTexture);
		uDiffuseSampler.Apply(cs);
		uSpecularSampler.SetTexture(material->specularTexture);
		uSpecularSampler.Apply(cs);
		uNormalSampler.SetTexture(material->normalTexture);
		uNormalSampler.Apply(cs);
		uDiffuse.SetValue(material->diffuse);
		uSpecular.SetValue(material->specular);
		uNormalCoordTransform.SetValue(material->normalCoordTransform);
		ugMaterial->Upload(context);

		// рисуем инстансингом обычные модели
		// установить пиксельный шейдер
		cs.pixelShader = GetPixelShader(PixelShaderKey(basicLightsCount, shadowLightsCount, false, material->GetKey()));
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
			cs.vertexBuffers[0] = geometry->GetVertexBuffer();
			cs.indexBuffer = geometry->GetIndexBuffer();

			// установить uniform'ы
			for(int k = 0; k < geometryBatchCount; ++k)
				uWorlds.SetValue(k, models[i + j + k].worldTransform);
			ugInstancedModel->Upload(context);

			// нарисовать
			context->DrawInstanced(geometryBatchCount);

			j += geometryBatchCount;
		}

		i += materialBatchCount;
	}

	//** нарисовать skinned-модели

	std::sort(skinnedModels.begin(), skinnedModels.end(), Sorter());

	// установить привязку атрибутов
	cs.attributeBinding = abSkinned;
	// установить вершинный шейдер
	cs.vertexShader = GetVertexShader(VertexShaderKey(false, true, false));
	// установить константный буфер
	ugSkinnedModel->Apply(cs);

	// нарисовать
	ptr<Material> lastMaterial;
	ptr<Geometry> lastGeometry;
	for(size_t i = 0; i < skinnedModels.size(); ++i)
	{
		const SkinnedModel& skinnedModel = skinnedModels[i];

		// установить параметры материала, если изменился
		ptr<Material> material = skinnedModel.material;
		if(lastMaterial != material)
		{
			uDiffuseSampler.SetTexture(material->diffuseTexture);
			uDiffuseSampler.Apply(cs);
			uSpecularSampler.SetTexture(material->specularTexture);
			uSpecularSampler.Apply(cs);
			uNormalSampler.SetTexture(material->normalTexture);
			uNormalSampler.Apply(cs);
			uDiffuse.SetValue(material->diffuse);
			uSpecular.SetValue(material->specular);
			uNormalCoordTransform.SetValue(material->normalCoordTransform);
			ugMaterial->Upload(context);

			lastMaterial = material;
		}

		// установить пиксельный шейдер
		cs.pixelShader = GetPixelShader(PixelShaderKey(basicLightsCount, shadowLightsCount, false, material->GetKey()));

		// установить геометрию, если изменилась
		ptr<Geometry> geometry = skinnedModel.geometry;
		if(lastGeometry != geometry)
		{
			cs.vertexBuffers[0] = geometry->GetVertexBuffer();
			cs.indexBuffer = geometry->GetIndexBuffer();
			lastGeometry = geometry;
		}

		// установить uniform'ы костей
		ptr<BoneAnimationFrame> animationFrame = skinnedModel.animationFrame;
		const std::vector<quat>& orientations = animationFrame->orientations;
		const std::vector<vec3>& offsets = animationFrame->offsets;
		int bonesCount = (int)orientations.size();
#ifdef _DEBUG
		if(bonesCount > maxBonesCount)
			THROW_PRIMARY_EXCEPTION("Too many bones");
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

	//** нарисовать декали

	std::stable_sort(decals.begin(), decals.end(), Sorter());

	// установить вершинный шейдер
	cs.vertexShader = GetVertexShader(VertexShaderKey(true, false, true));
	// установить константный буфер
	ugDecal->Apply(cs);
	// установить геометрию
	cs.attributeBinding = decalStuff.ab;
	cs.vertexBuffers[0] = decalStuff.vb;
	cs.indexBuffer = decalStuff.ib;
	// состояние смешивания
	cs.blendState = decalStuff.bs;
	// убрать карту нормалей и буфер глубины
	cs.renderBuffers[1] = 0;
	cs.depthStencilBuffer = 0;
	// семплеры
	uScreenNormalSampler.Apply(cs);
	uScreenDepthSampler.Apply(cs);

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
		uDiffuseSampler.SetTexture(material->diffuseTexture);
		uDiffuseSampler.Apply(cs);
		uSpecularSampler.SetTexture(material->specularTexture);
		uSpecularSampler.Apply(cs);
		uNormalSampler.SetTexture(material->normalTexture);
		uNormalSampler.Apply(cs);
		uDiffuse.SetValue(material->diffuse);
		uSpecular.SetValue(material->specular);
		uNormalCoordTransform.SetValue(material->normalCoordTransform);
		ugMaterial->Upload(context);

		// рисуем инстансингом декали
		// установить пиксельный шейдер
		cs.pixelShader = GetPixelShader(PixelShaderKey(basicLightsCount, shadowLightsCount, true, material->GetKey()));

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

	// всё, теперь постпроцессинг
	float clearColor[] = { 0, 0, 0, 0 };

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
		cs = csDownsamples[i];
		if(veryFirstDownsampling || i < downsamplingPassesCount - 1)
			context->ClearRenderBuffer(rbDownsamples[i], clearColor);
		context->Draw();
	}
	veryFirstDownsampling = false;

	// bloom
	uBloomLimit.SetValue(bloomLimit);
	ugBloom->Upload(context);

	const int bloomPassesCount = 5;

	bool enableBloom = true;

	if(enableBloom)
	{
		cs = csBloomLimit;
		context->ClearRenderBuffer(rbBloom2, clearColor);
		context->Draw();
		cs = csBloom2;
		context->ClearRenderBuffer(rbBloom1, clearColor);
		context->Draw();
		for(int i = 1; i < bloomPassesCount; ++i)
		{
			cs = csBloom1;
			context->ClearRenderBuffer(rbBloom2, clearColor);
			context->Draw();
			cs = csBloom2;
			context->ClearRenderBuffer(rbBloom1, clearColor);
			context->Draw();
		}
	}
	else
	{
		context->ClearRenderBuffer(rbBloom1, clearColor);
	}

	// tone mapping
	uToneLuminanceKey.SetValue(toneLuminanceKey);
	uToneMaxLuminance.SetValue(toneMaxLuminance);
	ugTone->Upload(context);
	cs = csTone;
	context->ClearRenderBuffer(rbBack, zeroColor);
	context->Draw();
}
