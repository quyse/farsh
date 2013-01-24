#include "Painter.hpp"
#include "BoneAnimation.hpp"

// говно FIXME HACK
#include "../inanity2/graphics/d3dx.hpp"

const int Painter::shadowMapSize = 512;
const int Painter::randomMapSize = 64;
const int Painter::downsamplingStepForBloom = 1;
const int Painter::bloomMapSize = 1 << (Painter::downsamplingPassesCount - 1 - Painter::downsamplingStepForBloom);

//*** Painter::BasicLight

Painter::BasicLight::BasicLight(ptr<UniformGroup> ug) :
	uLightPosition(ug->AddUniform<float3>()),
	uLightColor(ug->AddUniform<float3>())
{}

//*** Painter::ShadowLight

Painter::ShadowLight::ShadowLight(ptr<UniformGroup> ug, int samplerNumber) :
	BasicLight(ug),
	uLightTransform(ug->AddUniform<float4x4>()),
	uShadowSampler(samplerNumber)
{}

// Painter::LightVariant

Painter::LightVariant::LightVariant() :
	ugLight(NEW(UniformGroup(1))),
	uAmbientColor(ugLight->AddUniform<float3>())
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

Painter::Model::Model(ptr<Material> material, ptr<Geometry> geometry, const float4x4& worldTransform)
: material(material), geometry(geometry), worldTransform(worldTransform) {}

//*** Painter::SkinnedModel

Painter::SkinnedModel::SkinnedModel(ptr<Material> material, ptr<Geometry> geometry, ptr<Geometry> shadowGeometry, ptr<BoneAnimationFrame> animationFrame)
: material(material), geometry(geometry), shadowGeometry(shadowGeometry), animationFrame(animationFrame) {}

//*** Painter::Decal

Painter::Decal::Decal(ptr<Material> material, const float4x4& transform, const float4x4& invTransform)
: material(material), transform(transform), invTransform(invTransform) {}

//*** Painter::Light

Painter::Light::Light(const float3& position, const float3& color)
: position(position), color(color), shadow(false) {}

Painter::Light::Light(const float3& position, const float3& color, const float4x4& transform)
: position(position), color(color), transform(transform), shadow(true) {}

//*** Painter

Painter::Painter(ptr<Device> device, ptr<Context> context, ptr<Presenter> presenter, int screenWidth, int screenHeight, ptr<ShaderCache> shaderCache) :
	device(device),
	context(context),
	presenter(presenter),
	screenWidth(screenWidth),
	screenHeight(screenHeight),
	shaderCache(shaderCache),

	aPosition(0),
	aNormal(1),
	aTexcoord(2),
	aBoneNumbers(3),
	aBoneWeights(4),

	ugCamera(NEW(UniformGroup(0))),
	uViewProj(ugCamera->AddUniform<float4x4>()),
	uInvViewProj(ugCamera->AddUniform<float4x4>()),
	uCameraPosition(ugCamera->AddUniform<float3>()),

	ugMaterial(NEW(UniformGroup(2))),
	uDiffuse(ugMaterial->AddUniform<float4>()),
	uSpecular(ugMaterial->AddUniform<float4>()),
	uNormalCoordTransform(ugMaterial->AddUniform<float4>()),
	uDiffuseSampler(0),
	uSpecularSampler(1),
	uNormalSampler(2),

	ugModel(NEW(UniformGroup(3))),
	uWorld(ugModel->AddUniform<float4x4>()),

	ugInstancedModel(NEW(UniformGroup(3))),
	uWorlds(ugInstancedModel->AddUniformArray<float4x4>(maxInstancesCount)),

	ugSkinnedModel(NEW(UniformGroup(3))),
	uBoneOrientations(ugSkinnedModel->AddUniformArray<float4>(maxBonesCount)),
	uBoneOffsets(ugSkinnedModel->AddUniformArray<float4>(maxBonesCount)),

	ugDecal(NEW(UniformGroup(3))),
	uDecalTransforms(ugDecal->AddUniformArray<float4x4>(maxDecalsCount)),
	uDecalInvTransforms(ugDecal->AddUniformArray<float4x4>(maxDecalsCount)),
	uScreenNormalSampler(3),
	uScreenDepthSampler(4),

	ugShadowBlur(NEW(UniformGroup(0))),
	uShadowBlurDirection(ugShadowBlur->AddUniform<float2>()),
	uShadowBlurSourceSampler(0),

	ugDownsample(NEW(UniformGroup(0))),
	uDownsampleOffsets(ugDownsample->AddUniform<float4>()),
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

	ubCamera(device->CreateUniformBuffer(ugCamera->GetSize())),
	ubMaterial(device->CreateUniformBuffer(ugMaterial->GetSize())),
	ubModel(device->CreateUniformBuffer(ugModel->GetSize())),
	ubInstancedModel(device->CreateUniformBuffer(ugInstancedModel->GetSize())),
	ubSkinnedModel(device->CreateUniformBuffer(ugSkinnedModel->GetSize())),
	ubDecal(device->CreateUniformBuffer(ugDecal->GetSize())),
	ubShadowBlur(device->CreateUniformBuffer(ugShadowBlur->GetSize())),
	ubDownsample(device->CreateUniformBuffer(ugDownsample->GetSize())),
	ubBloom(device->CreateUniformBuffer(ugBloom->GetSize())),
	ubTone(device->CreateUniformBuffer(ugTone->GetSize())),

	iPosition(Semantics::VertexPosition),
	iNormal(Semantics::Custom(0)),
	iTexcoord(Semantics::Custom(1)),
	iWorldPosition(Semantics::Custom(2)),
	iDepth(Semantics::Custom(3)),
	iScreen(Semantics::Custom(4)),
	iInstance(Semantics::Custom(5)),

	fTarget(Semantics::TargetColor0),
	fNormal(Semantic(Semantics::TargetColor0 + 1))

{
	// финализировать uniform группы
	ugCamera->Finalize();
	ugMaterial->Finalize();
	ugModel->Finalize();
	ugInstancedModel->Finalize();
	ugSkinnedModel->Finalize();
	ugDecal->Finalize();
	ugShadowBlur->Finalize();
	ugDownsample->Finalize();
	ugBloom->Finalize();
	ugTone->Finalize();

	// создать ресурсы
	// запомнить размеры
	this->screenWidth = screenWidth;
	this->screenHeight = screenHeight;

	//** создать ресурсы
	rbBack = presenter->GetBackBuffer();
	dsbDepth = device->CreateDepthStencilBuffer(screenWidth, screenHeight, true);
	dsbShadow = device->CreateDepthStencilBuffer(shadowMapSize, shadowMapSize);
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

	// создать геометрию декалей
	{
		struct Vertex
		{
			float4 position;
			float3 normal;
			float2 texcoord;
		};
		Vertex vertices[] =
		{
			{ float4(0, 0, 0, 1), float3(0, 0, 1), float2(0, 0) },
			{ float4(1, 0, 0, 1), float3(0, 0, 1), float2(1, 0) },
			{ float4(1, 1, 0, 1), float3(0, 0, 1), float2(1, 1) },
			{ float4(0, 1, 0, 1), float3(0, 0, 1), float2(0, 1) },
			{ float4(0, 0, 1, 1), float3(0, 0, 1), float2(0, 0) },
			{ float4(1, 0, 1, 1), float3(0, 0, 1), float2(1, 0) },
			{ float4(1, 1, 1, 1), float3(0, 0, 1), float2(1, 1) },
			{ float4(0, 1, 1, 1), float3(0, 0, 1), float2(0, 1) }
		};
		unsigned short indices[] =
		{
			0, 2, 1, 0, 3, 2,
			0, 1, 5, 0, 5, 4,
			1, 2, 6, 1, 6, 5,
			2, 3, 7, 2, 7, 6,
			3, 0, 4, 3, 4, 7
		};

		std::vector<Layout::Element> layoutElements;
		layoutElements.push_back(Layout::Element(DataTypes::Float4, 0, 0));
		layoutElements.push_back(Layout::Element(DataTypes::Float3, 16, 1));
		layoutElements.push_back(Layout::Element(DataTypes::Float2, 28, 2));
		ptr<Layout> layout = NEW(Layout(layoutElements, sizeof(Vertex)));

		geometryDecal = NEW(Geometry(
			device->CreateVertexBuffer(MemoryFile::CreateViaCopy(vertices, sizeof(vertices)), layout),
			device->CreateIndexBuffer(MemoryFile::CreateViaCopy(indices, sizeof(indices)), sizeof(unsigned short))
		));
	}

	// состояние смешивания для декалей
	bsDecal = device->CreateBlendState();
	bsDecal->SetColor(BlendState::colorSourceSrcAlpha, BlendState::colorSourceInvSrcAlpha, BlendState::operationAdd);

	// геометрия полноэкранного прохода
	ptr<VertexBuffer> quadVertexBuffer;
	ptr<IndexBuffer> quadIndexBuffer;
	{
		// вершина для фильтра
		struct Vertex
		{
			float4 position;
			float2 texcoord;
			float2 gap;
		};
		// геометрия полноэкранного квадрата
		Vertex vertices[] =
		{
			{ float4(-1, -1, 0, 1), float2(0, 1) },
			{ float4(1, -1, 0, 1), float2(1, 1) },
			{ float4(1, 1, 0, 1), float2(1, 0) },
			{ float4(-1, 1, 0, 1), float2(0, 0) }
		};
		unsigned short indices[] = { 0, 2, 1, 0, 3, 2 };

		// разметка геометрии
		std::vector<Layout::Element> layoutElements;
		layoutElements.push_back(Layout::Element(DataTypes::Float4, 0, 0));
		layoutElements.push_back(Layout::Element(DataTypes::Float2, 16, 1));
		ptr<Layout> layout = NEW(Layout(layoutElements, sizeof(Vertex)));

		quadVertexBuffer = device->CreateVertexBuffer(MemoryFile::CreateViaCopy(vertices, sizeof(vertices)), layout);
		quadIndexBuffer = device->CreateIndexBuffer(MemoryFile::CreateViaCopy(indices, sizeof(indices)), sizeof(unsigned short));
	}

	//** инициализировать состояния конвейера

	// shadow pass
	csShadow.viewportWidth = shadowMapSize;
	csShadow.viewportHeight = shadowMapSize;
	csShadow.depthStencilBuffer = dsbShadow;
	csShadow.uniformBuffers[ugCamera->GetSlot()] = ubCamera;

	// пиксельный шейдер для теней
	csShadow.pixelShader = shaderCache->GetPixelShader(Expression((
		iPosition,
		iDepth,
		fTarget = newfloat4(iDepth, 0, 0, 0)
		)));

	//** шейдеры и состояния постпроцессинга и размытия теней
	{
		ContextState csFilter;
		csFilter.viewportWidth = screenWidth;
		csFilter.viewportHeight = screenHeight;
		csFilter.vertexBuffer = quadVertexBuffer;
		csFilter.indexBuffer = quadIndexBuffer;

		// атрибуты
		Attribute<float4> aPosition(0);
		Attribute<float2> aTexcoord(1);
		// промежуточные
		Interpolant<float4> iPosition(Semantics::VertexPosition);
		Interpolant<float2> iTexcoord(Semantics::Custom(0));
		// результат
		Fragment<float4> fTarget(Semantics::TargetColor0);

		// вершинный шейдер - общий для всех постпроцессингов
		ptr<VertexShader> vertexShader = shaderCache->GetVertexShader((
			iPosition = aPosition,
			iTexcoord = aTexcoord
			));

		csFilter.vertexShader = vertexShader;

		// пиксельный шейдер для размытия тени
		ptr<PixelShader> psShadowBlur;
		{
			Temp<float> sum;
			Expression shader = (
				iPosition,
				iTexcoord,
				sum = 0
				);
			static const float taps[] = { 0.006f, 0.061f, 0.242f, 0.383f, 0.242f, 0.061f, 0.006f };
			for(int i = 0; i < sizeof(taps) / sizeof(taps[0]); ++i)
				shader.Append((
					sum = sum + exp(uShadowBlurSourceSampler.Sample(iTexcoord + uShadowBlurDirection * Value<float>((float)i - 3))) * Value<float>(taps[i])
					));
			shader.Append((
				fTarget = newfloat4(log(sum), 0, 0, 1)
				));
			psShadowBlur = shaderCache->GetPixelShader(shader);
		}

		// пиксельный шейдер для downsample
		ptr<PixelShader> psDownsample;
		{
			Expression shader = (
				iPosition,
				iTexcoord,
				fTarget = newfloat4((
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("xz")) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("xw")) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("yz")) +
					uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("yw"))
					) * Value<float>(0.25f), 1.0f)
				);
			psDownsample = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для первого downsample luminance
		ptr<PixelShader> psDownsampleLuminanceFirst;
		{
			Temp<float3> luminanceCoef;
			Expression shader = (
				iPosition,
				iTexcoord,
				luminanceCoef = newfloat3(0.2126f, 0.7152f, 0.0722f),
				fTarget = newfloat4((
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("xz")), luminanceCoef) + Value<float>(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("xw")), luminanceCoef) + Value<float>(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("yz")), luminanceCoef) + Value<float>(0.0001f)) +
					log(dot(uDownsampleSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("yw")), luminanceCoef) + Value<float>(0.0001f))
					) * Value<float>(0.25f), 0.0f, 0.0f, 1.0f)
				);
			psDownsampleLuminanceFirst = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для downsample luminance
		ptr<PixelShader> psDownsampleLuminance;
		{
			Expression shader = (
				iPosition,
				iTexcoord,
				fTarget = newfloat4((
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("xz")) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("xw")) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("yz")) +
					uDownsampleLuminanceSourceSampler.Sample(iTexcoord + uDownsampleOffsets.Swizzle<float2>("yw"))
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
			Temp<float3> sum;
			Expression shader = (
				iPosition,
				iTexcoord,
				sum = newfloat3(0, 0, 0)
				);
			for(int i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
			{
				shader.Append((
					sum = sum + max(uBloomSourceSampler.Sample(iTexcoord + newfloat2(offsets[i] * offsetScaleX, 0)) - uBloomLimit, newfloat3(0, 0, 0))
					));
			}
			shader.Append((
				fTarget = newfloat4(sum * Value<float>(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f)
				));
			psBloomLimit = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для первого прохода
		ptr<PixelShader> psBloom1;
		{
			Temp<float3> sum;
			Expression shader = (
				iPosition,
				iTexcoord,
				sum = newfloat3(0, 0, 0)
				);
			for(int i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
			{
				shader.Append((
					sum = sum + uBloomSourceSampler.Sample(iTexcoord + newfloat2(offsets[i] * offsetScaleX, 0))
					));
			}
			shader.Append((
				fTarget = newfloat4(sum * Value<float>(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f)
				));
			psBloom1 = shaderCache->GetPixelShader(shader);
		}
		// пиксельный шейдер для второго прохода
		ptr<PixelShader> psBloom2;
		{
			Temp<float3> sum;
			Expression shader = (
				iPosition,
				iTexcoord,
				sum = newfloat3(0, 0, 0)
				);
			for(int i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
			{
				shader.Append((
					sum = sum + uBloomSourceSampler.Sample(iTexcoord + newfloat2(0, offsets[i] * offsetScaleY))
					));
			}
			shader.Append((
				fTarget = newfloat4(sum * Value<float>(1.0f / (sizeof(offsets) / sizeof(offsets[0]))), 1.0f)
				));
			psBloom2 = shaderCache->GetPixelShader(shader);
		}
		// шейдер tone mapping
		ptr<PixelShader> psTone;
		{
			Temp<float3> color;
			Temp<float> luminance, relativeLuminance, intensity;
			Expression shader = (
				iPosition,
				iTexcoord,
				color = uToneScreenSampler.Sample(iTexcoord) + uToneBloomSampler.Sample(iTexcoord),
				luminance = dot(color, newfloat3(0.2126f, 0.7152f, 0.0722f)),
				relativeLuminance = uToneLuminanceKey * luminance / exp(uToneAverageSampler.Sample(newfloat2(0.5f, 0.5f))),
				intensity = relativeLuminance * (Value<float>(1) + relativeLuminance / uToneMaxLuminance) / (Value<float>(1) + relativeLuminance),
				color = saturate(color * (intensity / luminance)),
				// гамма-коррекция
				color = pow(color, 0.45f),
				fTarget = newfloat4(color, 1.0f)
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
		csShadowBlur.uniformBuffers[ugShadowBlur->GetSlot()] = ubShadowBlur;
		csShadowBlur.pixelShader = psShadowBlur;

		// проходы даунсемплинга
		for(int i = 0; i < downsamplingPassesCount; ++i)
		{
			ContextState& cs = csDownsamples[i];
			cs = csFilter;

			cs.renderBuffers[0] = rbDownsamples[i];
			cs.viewportWidth = 1 << (downsamplingPassesCount - 1 - i);
			cs.viewportHeight = 1 << (downsamplingPassesCount - 1 - i);
			cs.uniformBuffers[ugDownsample->GetSlot()] = ubDownsample;
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
		csBloomLimit.uniformBuffers[ugBloom->GetSlot()] = ubBloom;
		csBloomLimit.pixelShader = psBloomLimit;
		// первый проход bloom (из rbBloom1 в rbBloom2)
		csBloom1.viewportWidth = bloomMapSize;
		csBloom1.viewportHeight = bloomMapSize;
		csBloom1.renderBuffers[0] = rbBloom2;
		uBloomSourceSampler.SetTexture(rbBloom1->GetTexture());
		uBloomSourceSampler.SetSamplerState(linearSampler);
		uBloomSourceSampler.Apply(csBloom1);
		csBloom1.uniformBuffers[ugBloom->GetSlot()] = ubBloom;
		csBloom1.pixelShader = psBloom1;
		// второй проход bloom (из rbBloom2 в rbBloom1)
		csBloom2.viewportWidth = bloomMapSize;
		csBloom2.viewportHeight = bloomMapSize;
		csBloom2.renderBuffers[0] = rbBloom1;
		uBloomSourceSampler.SetTexture(rbBloom2->GetTexture());
		uBloomSourceSampler.SetSamplerState(linearSampler);
		uBloomSourceSampler.Apply(csBloom2);
		csBloom2.uniformBuffers[ugBloom->GetSlot()] = ubBloom;
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
		csTone.uniformBuffers[ugTone->GetSlot()] = ubTone;
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

	lightVariant.ugLight->Finalize();

	// создать uniform-буфер для параметров
	lightVariant.ubLight = device->CreateUniformBuffer(lightVariant.ugLight->GetSize());

	// инициализировать состояние контекста
	ContextState& cs = lightVariant.csOpaque;
	cs.viewportWidth = screenWidth;
	cs.viewportHeight = screenHeight;
	cs.renderBuffers[0] = rbScreen;
	cs.renderBuffers[1] = rbScreenNormal;
	cs.depthStencilBuffer = dsbDepth;
	cs.uniformBuffers[ugCamera->GetSlot()] = ubCamera;
	cs.uniformBuffers[lightVariant.ugLight->GetSlot()] = lightVariant.ubLight;
	cs.uniformBuffers[ugMaterial->GetSlot()] = ubMaterial;

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

Value<float3> Painter::ApplyQuaternion(Value<float4> q, Value<float3> v)
{
	return v + cross(q.Swizzle<float3>("xyz"), cross(q.Swizzle<float3>("xyz"), v) + v * q.Swizzle<float>("w")) * Value<float>(2);
}

Expression Painter::GetWorldPositionAndNormal(const VertexShaderKey& key)
{
	if(key.skinned)
	{
		Value<float3> position = aPosition.Swizzle<float3>("xyz");
		Value<uint> boneNumbers[] =
		{
			aBoneNumbers.Swizzle<uint>("x"),
			aBoneNumbers.Swizzle<uint>("y"),
			aBoneNumbers.Swizzle<uint>("z"),
			aBoneNumbers.Swizzle<uint>("w")
		};
		Value<float> boneWeights[] =
		{
			aBoneWeights.Swizzle<float>("x"),
			aBoneWeights.Swizzle<float>("y"),
			aBoneWeights.Swizzle<float>("z"),
			aBoneWeights.Swizzle<float>("w")
		};
		Temp<float3> tmpBoneOffsets[4];

		return
			tmpBoneOffsets[0] = uBoneOffsets[boneNumbers[0]].Swizzle<float3>("xyz"),
			tmpBoneOffsets[1] = uBoneOffsets[boneNumbers[1]].Swizzle<float3>("xyz"),
			tmpBoneOffsets[2] = uBoneOffsets[boneNumbers[2]].Swizzle<float3>("xyz"),
			tmpBoneOffsets[3] = uBoneOffsets[boneNumbers[3]].Swizzle<float3>("xyz"),
			tmpVertexPosition = newfloat4(
				(ApplyQuaternion(uBoneOrientations[boneNumbers[0]], position) + tmpBoneOffsets[0]) * boneWeights[0] +
				(ApplyQuaternion(uBoneOrientations[boneNumbers[1]], position) + tmpBoneOffsets[1]) * boneWeights[1] +
				(ApplyQuaternion(uBoneOrientations[boneNumbers[2]], position) + tmpBoneOffsets[2]) * boneWeights[2] +
				(ApplyQuaternion(uBoneOrientations[boneNumbers[3]], position) + tmpBoneOffsets[3]) * boneWeights[3],
				1.0f),
			tmpVertexNormal =
				ApplyQuaternion(uBoneOrientations[boneNumbers[0]], aNormal) * boneWeights[0] +
				ApplyQuaternion(uBoneOrientations[boneNumbers[1]], aNormal) * boneWeights[1] +
				ApplyQuaternion(uBoneOrientations[boneNumbers[2]], aNormal) * boneWeights[2] +
				ApplyQuaternion(uBoneOrientations[boneNumbers[3]], aNormal) * boneWeights[3];
	}
	else
	{
		Temp<float4x4> tmpWorld;
		Temp<float4> tmpPosition;
		Temp<uint> tmpInstance;
		Expression e((
			key.instanced ?
			(
				tmpInstance = Value<uint>(NEW(SpecialNode(DataTypes::UInt, Semantics::Instance))),
				iInstance = tmpInstance,
				(
					key.decal ?
					(
						tmpWorld = uDecalInvTransforms[tmpInstance],
						tmpPosition = mul(aPosition, tmpWorld),
						tmpPosition = tmpPosition / tmpPosition.Swizzle<float>("w")
					)
					:
					(
						tmpWorld = uWorlds[tmpInstance],
						tmpPosition = mul(aPosition, tmpWorld)
					)
				)
			)
			:
			(
				tmpWorld = uWorld,
				tmpPosition = mul(aPosition, tmpWorld)
			)
		));

		return
			e,
			tmpVertexPosition = Value<float4>(tmpPosition),
			tmpVertexNormal = mul(aNormal, tmpWorld.Cast<float3x3>());
	}
}

Expression Painter::BeginMaterialLighting(const PixelShaderKey& key, Value<float3> ambientColor)
{
	Expression e =
		tmpWorldPosition = newfloat4(iWorldPosition, 1.0f);

	// получить текстурные координаты
	if(key.decal)
	{
		Temp<float4> tmpScreen, tmpProjectedPosition;
		Temp<float2> tmpScreenCoords;
		Temp<float> tmpScreenDepth;
		e.Append((
			// получить спроецированную позицию
			tmpScreen = iScreen / iScreen.Swizzle<float>("w"),
			tmpScreenCoords = newfloat2(
				tmpScreen.Swizzle<float>("x") + Value<float>(1),
				- tmpScreen.Swizzle<float>("y") + Value<float>(1)) * Value<float>(0.5f),
			tmpScreenDepth = uScreenDepthSampler.Sample(tmpScreenCoords),
			tmpProjectedPosition = mul(newfloat4(tmpScreen.Swizzle<float2>("xy"), tmpScreenDepth, 1), uInvViewProj),
			tmpProjectedPosition = tmpProjectedPosition / tmpProjectedPosition.Swizzle<float>("w"),
			// преобразовать эту позицию в пространство декали
			tmpProjectedPosition = mul(tmpProjectedPosition, uDecalTransforms[iInstance]),
			tmpProjectedPosition = tmpProjectedPosition / tmpProjectedPosition.Swizzle<float>("w"),
			//clip(tmpProjectedPosition.Swizzle<float>("z")),
			//clip(Value<float>(1) - tmpProjectedPosition.Swizzle<float>("z")),
			// получить текстурные координаты
			tmpTexcoord = newfloat2(
				tmpProjectedPosition.Swizzle<float>("x") + Value<float>(1),
				-tmpProjectedPosition.Swizzle<float>("y") + Value<float>(1)) * Value<float>(0.5f),

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
			Temp<float3> dxPosition, dyPosition;
			Temp<float2> dxTexcoord, dyTexcoord;
			Temp<float3> r0, r1, r2, T1, T2, T3;
			Temp<float3> perturbedNormal;
			e.Append((
				dxPosition = ddx(iWorldPosition),
				dyPosition = ddy(iWorldPosition),
				dxTexcoord = ddx(tmpTexcoord),
				dyTexcoord = ddy(tmpTexcoord),

				r0 = cross(dxPosition, dyPosition),

				r1 = cross(dyPosition, r0),
				r2 = cross(r0, dxPosition),

				T1 = normalize(r1 * dxTexcoord.Swizzle<float>("x") + r2 * dyTexcoord.Swizzle<float>("x")),
				T2 = normalize(r1 * dxTexcoord.Swizzle<float>("y") + r2 * dyTexcoord.Swizzle<float>("y")),
				T3 = normalize(iNormal),

				perturbedNormal = (uNormalSampler.Sample(tmpTexcoord * uNormalCoordTransform.Swizzle<float2>("xy") + uNormalCoordTransform.Swizzle<float2>("zw")) * Value<float>(2) - Value<float>(1)),
				tmpNormal = normalize(T1 * perturbedNormal.Swizzle<float>("x") + T2 * perturbedNormal.Swizzle<float>("y") + T3 * perturbedNormal.Swizzle<float>("z"))
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
		tmpSpecularExponent = exp2(tmpSpecular.Swizzle<float>("x") * Value<float>(4/*12*/)),
		tmpColor = ambientColor * tmpDiffuse.Swizzle<float3>("xyz")
	));

	return e;
}

Expression Painter::ApplyMaterialLighting(Value<float3> lightPosition, Value<float3> lightColor)
{
	Temp<float3> tmpToLight, tmpLightViewBissect, tmpDiffusePart, tmpSpecularPart;
	return
		// направление на свет
		tmpToLight = normalize(lightPosition - iWorldPosition),
		// биссектриса между направлениями на свет и камеру
		tmpLightViewBissect = normalize(tmpToLight + tmpToCamera),
		// диффузная составляющая
		tmpDiffusePart = tmpDiffuse.Swizzle<float3>("xyz")
			* max(dot(tmpNormal, tmpToLight), 0),
		// specular составляющая
		tmpSpecularPart = tmpDiffuse.Swizzle<float3>("xyz")
			* pow(max(dot(tmpLightViewBissect, tmpNormal), 0), tmpSpecularExponent)
			//* dot(tmpNormal, tmpToLight) // хз, может не нужно оно?
			* (tmpSpecularExponent + Value<float>(1)) / (max(pow(dot(tmpToLight, tmpLightViewBissect), 3), 0.1f) * Value<float>(8)),
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

	Temp<float4> p;

	Expression e((
		GetWorldPositionAndNormal(key),
		p = mul(tmpVertexPosition, uViewProj),
		iPosition = p,
		iNormal = tmpVertexNormal,
		iTexcoord = aTexcoord,
		iWorldPosition = tmpVertexPosition.Swizzle<float3>("xyz")
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

	ptr<VertexShader> vertexShader = shaderCache->GetVertexShader(Expression((
		GetWorldPositionAndNormal(key),
		iPosition = mul(tmpVertexPosition, uViewProj),
		iDepth = iPosition.Swizzle<float>("z")
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
		iPosition,
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
		Temp<float4> shadowCoords;
		Temp<float> shadowMultiplier;
		Temp<float2> shadowCoordsXY;
		Temp<float> linearShadowZ;
		Temp<float> lighted;
		shader.Append((
			shadowCoords = mul(tmpWorldPosition, shadowLight.uLightTransform),
			lighted = shadowCoords.Swizzle<float>("z") > Value<float>(0),
			linearShadowZ = shadowCoords.Swizzle<float>("z"),
			//lighted = lighted * (linearShadowZ > Value<float>(0)),
			shadowCoords = shadowCoords / shadowCoords.Swizzle<float>("w"),
			lighted = lighted * (abs(shadowCoords.Swizzle<float>("x")) < Value<float>(1)) * (abs(shadowCoords.Swizzle<float>("y")) < Value<float>(1)),
			shadowCoordsXY = newfloat2(
				(shadowCoords.Swizzle<float>("x") + Value<float>(1.0f)) * Value<float>(0.5f),
				(Value<float>(1.0f) - shadowCoords.Swizzle<float>("y")) * Value<float>(0.5f)),
			shadowMultiplier = lighted * saturate(exp(Value<float>(4) * (shadowLight.uShadowSampler.Sample(shadowCoordsXY) - linearShadowZ))),
			
			ApplyMaterialLighting(shadowLight.uLightPosition, shadowLight.uLightColor * shadowMultiplier)
			));
	}

	// вернуть цвет
	shader.Append((
		fTarget = newfloat4(tmpColor, tmpDiffuse.Swizzle<float>("w"))
	));
	// если не декали, вернуть нормаль
	if(!key.decal)
		shader.Append((
			fNormal = newfloat4((tmpNormal + Value<float>(1)) * Value<float>(0.5f), 1)
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

void Painter::SetCamera(const float4x4& cameraViewProj, const float3& cameraPosition)
{
	this->cameraViewProj = cameraViewProj;
	// полный отстой, но инвертирования матрицы пока нет
	D3DXMATRIX mxA((const float*)cameraViewProj.t), mxB;
	D3DXMatrixInverse(&mxB, NULL, &mxA);
	matrix<4, 4>& c = this->cameraInvViewProj;
	c.t[0][0] = mxB._11; c.t[0][1] = mxB._12; c.t[0][2] = mxB._13; c.t[0][3] = mxB._14;
	c.t[1][0] = mxB._21; c.t[1][1] = mxB._22; c.t[1][2] = mxB._23; c.t[1][3] = mxB._24;
	c.t[2][0] = mxB._31; c.t[2][1] = mxB._32; c.t[2][2] = mxB._33; c.t[2][3] = mxB._34;
	c.t[3][0] = mxB._41; c.t[3][1] = mxB._42; c.t[3][2] = mxB._43; c.t[3][3] = mxB._44;
	this->cameraPosition = cameraPosition;
}

void Painter::AddModel(ptr<Material> material, ptr<Geometry> geometry, const float4x4& worldTransform)
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

void Painter::AddDecal(ptr<Material> material, const float4x4& transform, const float4x4& invTransform)
{
	decals.push_back(Decal(material, transform, invTransform));
}

void Painter::SetAmbientColor(const float3& ambientColor)
{
	this->ambientColor = ambientColor;
}

void Painter::AddBasicLight(const float3& position, const float3& color)
{
	lights.push_back(Light(position, color));
}

void Painter::AddShadowLight(const float3& position, const float3& color, const float4x4& transform)
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
			context->SetUniformBufferData(ubCamera, ugCamera->GetData(), ugCamera->GetSize());

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

			// установить вершинный шейдер
			cs.vertexShader = GetVertexShadowShader(VertexShaderKey(true, false, false));
			// установить константный буфер
			cs.uniformBuffers[ugInstancedModel->GetSlot()] = ubInstancedModel;

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
				cs.vertexBuffer = models[j].geometry->GetVertexBuffer();
				cs.indexBuffer = models[j].geometry->GetIndexBuffer();
				// установить uniform'ы
				for(int k = 0; k < batchCount; ++k)
					uWorlds.SetValue(k, models[j + k].worldTransform);
				// и залить в GPU
				context->SetUniformBufferData(ubInstancedModel, ugInstancedModel->GetData(), ugInstancedModel->GetSize());

				// нарисовать
				context->DrawInstanced(batchCount);

				j += batchCount;
			}

			//** рисуем skinned-модели

			// отсортировать объекты по геометрии
			std::sort(skinnedModels.begin(), skinnedModels.end(), GeometrySorter());

			// установить вершинный шейдер
			cs.vertexShader = GetVertexShadowShader(VertexShaderKey(false, true, false));
			// установить константный буфер
			cs.uniformBuffers[ugSkinnedModel->GetSlot()] = ubSkinnedModel;

			// нарисовать с группировкой по геометрии
			ptr<Geometry> lastGeometry;
			for(size_t j = 0; j < skinnedModels.size(); ++j)
			{
				const SkinnedModel& skinnedModel = skinnedModels[j];
				// установить геометрию, если отличается
				if(lastGeometry != skinnedModel.shadowGeometry)
				{
					cs.vertexBuffer = skinnedModel.shadowGeometry->GetVertexBuffer();
					cs.indexBuffer = skinnedModel.shadowGeometry->GetIndexBuffer();
					lastGeometry = skinnedModel.shadowGeometry;
				}
				// установить uniform'ы костей
				ptr<BoneAnimationFrame> animationFrame = skinnedModel.animationFrame;
				const std::vector<quaternion>& orientations = animationFrame->orientations;
				const std::vector<float3>& offsets = animationFrame->offsets;
				int bonesCount = (int)orientations.size();
#ifdef _DEBUG
				if(bonesCount > maxBonesCount)
					THROW_PRIMARY_EXCEPTION("Too many bones");
#endif
				for(int k = 0; k < bonesCount; ++k)
				{
					uBoneOrientations.SetValue(k, orientations[k]);
					uBoneOffsets.SetValue(k, float4(offsets[k].x, offsets[k].y, offsets[k].z, 0));
				}
				// и залить в GPU
				context->SetUniformBufferData(ubSkinnedModel, ugSkinnedModel->GetData(), ugSkinnedModel->GetSize());

				// нарисовать
				context->Draw();
			}

			// выполнить размытие тени
			// первый проход
			cs = csShadowBlur;
			cs.renderBuffers[0] = rbShadowBlur;
			uShadowBlurSourceSampler.SetTexture(rb->GetTexture());
			uShadowBlurSourceSampler.Apply(cs);
			uShadowBlurDirection.SetValue(float2(1.0f / shadowMapSize, 0));
			context->SetUniformBufferData(ubShadowBlur, ugShadowBlur->GetData(), ugShadowBlur->GetSize());
			context->ClearRenderBuffer(rbShadowBlur, zeroColor);
			context->Draw();
			// второй проход
			cs = csShadowBlur;
			cs.renderBuffers[0] = rb;
			uShadowBlurSourceSampler.SetTexture(rbShadowBlur->GetTexture());
			uShadowBlurSourceSampler.Apply(cs);
			uShadowBlurDirection.SetValue(float2(0, 1.0f / shadowMapSize));
			context->SetUniformBufferData(ubShadowBlur, ugShadowBlur->GetData(), ugShadowBlur->GetSize());
			context->ClearRenderBuffer(rb, zeroColor);
			context->Draw();

			shadowPassNumber++;
		}

	// очистить рендербуферы
	float color[4] = { 0, 0, 0, 0 };
	float colorDepth[4] = { 1, 1, 1, 1 };
	context->ClearRenderBuffer(rbScreen, color);
	context->ClearRenderBuffer(rbScreenNormal, color);
	context->ClearDepthStencilBuffer(dsbDepth, 1.0f);

	ContextState& cs = context->GetTargetState();

	// установить uniform'ы камеры
	uViewProj.SetValue(cameraViewProj);
	uInvViewProj.SetValue(cameraInvViewProj);
	uCameraPosition.SetValue(cameraPosition);
	context->SetUniformBufferData(ubCamera, ugCamera->GetData(), ugCamera->GetSize());

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
	context->SetUniformBufferData(lightVariant.ubLight, lightVariant.ugLight->GetData(), lightVariant.ugLight->GetSize());

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

	// установить вершинный шейдер
	cs.vertexShader = GetVertexShader(VertexShaderKey(true, false, false));
	// установить константный буфер
	cs.uniformBuffers[ugInstancedModel->GetSlot()] = ubInstancedModel;

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
		context->SetUniformBufferData(ubMaterial, ugMaterial->GetData(), ugMaterial->GetSize());

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
			cs.vertexBuffer = geometry->GetVertexBuffer();
			cs.indexBuffer = geometry->GetIndexBuffer();

			// установить uniform'ы
			for(int k = 0; k < geometryBatchCount; ++k)
				uWorlds.SetValue(k, models[i + j + k].worldTransform);
			context->SetUniformBufferData(ubInstancedModel, ugInstancedModel->GetData(), ugInstancedModel->GetSize());

			// нарисовать
			context->DrawInstanced(geometryBatchCount);

			j += geometryBatchCount;
		}

		i += materialBatchCount;
	}

	//** нарисовать skinned-модели

	std::sort(skinnedModels.begin(), skinnedModels.end(), Sorter());

	// установить вершинный шейдер
	cs.vertexShader = GetVertexShader(VertexShaderKey(false, true, false));
	// установить константный буфер
	cs.uniformBuffers[ugSkinnedModel->GetSlot()] = ubSkinnedModel;

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
			context->SetUniformBufferData(ubMaterial, ugMaterial->GetData(), ugMaterial->GetSize());

			lastMaterial = material;
		}

		// установить пиксельный шейдер
		cs.pixelShader = GetPixelShader(PixelShaderKey(basicLightsCount, shadowLightsCount, false, material->GetKey()));

		// установить геометрию, если изменилась
		ptr<Geometry> geometry = skinnedModel.geometry;
		if(lastGeometry != geometry)
		{
			cs.vertexBuffer = geometry->GetVertexBuffer();
			cs.indexBuffer = geometry->GetIndexBuffer();
			lastGeometry = geometry;
		}

		// установить uniform'ы костей
		ptr<BoneAnimationFrame> animationFrame = skinnedModel.animationFrame;
		const std::vector<quaternion>& orientations = animationFrame->orientations;
		const std::vector<float3>& offsets = animationFrame->offsets;
		int bonesCount = (int)orientations.size();
#ifdef _DEBUG
		if(bonesCount > maxBonesCount)
			THROW_PRIMARY_EXCEPTION("Too many bones");
#endif
		for(int k = 0; k < bonesCount; ++k)
		{
			uBoneOrientations.SetValue(k, orientations[k]);
			uBoneOffsets.SetValue(k, float4(offsets[k].x, offsets[k].y, offsets[k].z, 0));
		}
		context->SetUniformBufferData(ubSkinnedModel, ugSkinnedModel->GetData(), ugSkinnedModel->GetSize());

		// нарисовать
		context->Draw();
	}

	//** нарисовать декали

	std::stable_sort(decals.begin(), decals.end(), Sorter());

	// установить вершинный шейдер
	cs.vertexShader = GetVertexShader(VertexShaderKey(true, false, true));
	// установить константный буфер
	cs.uniformBuffers[ugDecal->GetSlot()] = ubDecal;
	// установить геометрию
	cs.vertexBuffer = geometryDecal->GetVertexBuffer();
	cs.indexBuffer = geometryDecal->GetIndexBuffer();
	// состояние смешивания
	cs.blendState = bsDecal;
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
		context->SetUniformBufferData(ubMaterial, ugMaterial->GetData(), ugMaterial->GetSize());

		// рисуем инстансингом декали
		// установить пиксельный шейдер
		cs.pixelShader = GetPixelShader(PixelShaderKey(basicLightsCount, shadowLightsCount, true, material->GetKey()));

		// установить uniform'ы
		for(int j = 0; j < materialBatchCount; ++j)
		{
			uDecalTransforms.SetValue(j, decals[i + j].transform);
			uDecalInvTransforms.SetValue(j, decals[i + j].invTransform);
		}
		context->SetUniformBufferData(ubDecal, ugDecal->GetData(), ugDecal->GetSize());

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
		uDownsampleOffsets.SetValue(float4(-halfSourcePixelWidth, halfSourcePixelWidth, -halfSourcePixelHeight, halfSourcePixelHeight));
		context->SetUniformBufferData(ubDownsample, ugDownsample->GetData(), ugDownsample->GetSize());
		cs = csDownsamples[i];
		if(veryFirstDownsampling || i < downsamplingPassesCount - 1)
			context->ClearRenderBuffer(rbDownsamples[i], clearColor);
		context->Draw();
	}
	veryFirstDownsampling = false;

	// bloom
	uBloomLimit.SetValue(bloomLimit);
	context->SetUniformBufferData(ubBloom, ugBloom->GetData(), ugBloom->GetSize());

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
	context->SetUniformBufferData(ubTone, ugTone->GetData(), ugTone->GetSize());
	cs = csTone;
	context->ClearRenderBuffer(rbBack, clearColor);
	context->Draw();
}
