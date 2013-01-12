#include "Painter.hpp"

const int Painter::shadowMapSize = 512;
const int Painter::randomMapSize = 64;

Painter::BasicLight::BasicLight(ptr<UniformGroup> ug) :
	uLightPosition(ug->AddUniform<float3>()),
	uLightColor(ug->AddUniform<float3>())
{}

Painter::ShadowLight::ShadowLight(ptr<UniformGroup> ug, int samplerNumber) :
	BasicLight(ug),
	uLightTransform(ug->AddUniform<float4x4>()),
	uShadowSampler(samplerNumber)
{}

Painter::LightVariant::LightVariant() :
	ugLight(NEW(UniformGroup(1))),
	uAmbientColor(ugLight->AddUniform<float3>())
{}

Painter::ShaderKey::ShaderKey(int basicLightsCount, int shadowLightsCount, bool skinned) :
basicLightsCount(basicLightsCount), shadowLightsCount(shadowLightsCount), skinned(skinned)
{}

Painter::ShaderKey::operator size_t() const
{
	return basicLightsCount | (shadowLightsCount << 3) | (skinned << 6);
}

Painter::Shader::Shader() {}

Painter::Shader::Shader(ptr<VertexShader> vertexShader, ptr<PixelShader> pixelShader)
: vertexShader(vertexShader), pixelShader(pixelShader) {}

Painter::Model::Model(ptr<Texture> diffuseTexture, ptr<Texture> specularTexture, ptr<VertexBuffer> vertexBuffer, ptr<IndexBuffer> indexBuffer, const float4x4& worldTransform)
: diffuseTexture(diffuseTexture), specularTexture(specularTexture), vertexBuffer(vertexBuffer), indexBuffer(indexBuffer), worldTransform(worldTransform) {}

Painter::Light::Light(const float3& position, const float3& color)
: position(position), color(color), shadow(false) {}

Painter::Light::Light(const float3& position, const float3& color, const float4x4& transform)
: position(position), color(color), transform(transform), shadow(true) {}

Painter::Painter(ptr<Device> device, ptr<Context> context, ptr<Presenter> presenter, int screenWidth, int screenHeight) :
	device(device),
	context(context),
	presenter(presenter),
	screenWidth(screenWidth),
	screenHeight(screenHeight),

	aPosition(0),
	aNormal(1),
	aTexcoord(2),

	ugCamera(NEW(UniformGroup(0))),
	uViewProj(ugCamera->AddUniform<float4x4>()),
	uCameraPosition(ugCamera->AddUniform<float3>()),

	ugMaterial(NEW(UniformGroup(2))),
	uRandomSampler(0),
	uDiffuseSampler(1),
	uSpecularSampler(2),

	ugModel(NEW(UniformGroup(3))),
	uWorlds(ugModel->AddUniformArray<float4x4>(maxInstancesCount)),

	ugPostprocess(NEW(UniformGroup(0))),
	uBloomLimit(ugPostprocess->AddUniform<float>()),
	uAdaptationLuminance(ugPostprocess->AddUniform<float>()),
	uLuminanceKey(ugPostprocess->AddUniform<float>()),
	uMaxLuminance(ugPostprocess->AddUniform<float>()),
	uBloomSourceSampler(0),
	uScreenSampler(1),

	ubCamera(device->CreateUniformBuffer(ugCamera->GetSize())),
	ubMaterial(device->CreateUniformBuffer(ugMaterial->GetSize())),
	ubModel(device->CreateUniformBuffer(ugModel->GetSize())),
	ubPostprocess(device->CreateUniformBuffer(ugPostprocess->GetSize()))
{
	// финализировать uniform группы
	ugCamera->Finalize();
	ugMaterial->Finalize();
	ugModel->Finalize();
	ugPostprocess->Finalize();

	// создать ресурсы
	// запомнить размеры
	this->screenWidth = screenWidth;
	this->screenHeight = screenHeight;

	//** создать ресурсы
	rbBack = presenter->GetBackBuffer();
	dsbDepth = device->CreateDepthStencilBuffer(screenWidth, screenHeight);
	for(int i = 0; i < maxShadowLightsCount; ++i)
		dsbShadows[i] = device->CreateDepthStencilBuffer(shadowMapSize, shadowMapSize, true);

	// экранный буфер
	rbScreen = device->CreateRenderBuffer(screenWidth, screenHeight, PixelFormats::floatR11G11B10);
	// буферы для Bloom
	rbBloom1 = device->CreateRenderBuffer(screenWidth, screenHeight, PixelFormats::floatR11G11B10);
	rbBloom2 = device->CreateRenderBuffer(screenWidth, screenHeight, PixelFormats::floatR11G11B10);

	shadowSamplerState = device->CreateSamplerState();
	shadowSamplerState->SetWrap(SamplerState::wrapBorder, SamplerState::wrapBorder, SamplerState::wrapBorder);
	shadowSamplerState->SetFilter(SamplerState::filterLinear, SamplerState::filterLinear, SamplerState::filterLinear);
	{
		float borderColor[] = { 0, 0, 0, 0 };
		shadowSamplerState->SetBorderColor(borderColor);
	}

	// создать случайную текстуру
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
		uRandomSampler.SetTexture(randomTexture);
		ptr<SamplerState> ss = device->CreateSamplerState();
		ss->SetWrap(SamplerState::wrapRepeat, SamplerState::wrapRepeat, SamplerState::wrapRepeat);
		uRandomSampler.SetSamplerState(ss);
	}

	//** инициализировать состояния конвейера
	ContextState cleanState;
	if(0)
	{
		cleanState.blendState = device->CreateBlendState();
		cleanState.blendState->SetColor(BlendState::colorSourceSrcAlpha, BlendState::colorSourceInvSrcAlpha, BlendState::operationAdd);
		cleanState.blendState->SetAlpha(BlendState::alphaSourceOne, BlendState::alphaSourceZero, BlendState::operationAdd);
	}

	// shadow pass
	shadowContextState = cleanState;
	shadowContextState.viewportWidth = shadowMapSize;
	shadowContextState.viewportHeight = shadowMapSize;
	shadowContextState.uniformBuffers[ugCamera->GetSlot()] = ubCamera;
	shadowContextState.uniformBuffers[ugModel->GetSlot()] = ubModel;

	// варианты света
	for(int basicLightsCount = 0; basicLightsCount <= maxBasicLightsCount; ++basicLightsCount)
		for(int shadowLightsCount = 0; shadowLightsCount <= maxShadowLightsCount; ++shadowLightsCount)
		{
			LightVariant& lightVariant = lightVariants[basicLightsCount][shadowLightsCount];

			// инициализировать uniform'ы
			for(int i = 0; i < basicLightsCount; ++i)
				lightVariant.basicLights.push_back(BasicLight(lightVariant.ugLight));
			for(int i = 0; i < shadowLightsCount; ++i)
				// первые три семплера - для рандомной текстуры и материала
				lightVariant.shadowLights.push_back(ShadowLight(lightVariant.ugLight, i + 3));

			lightVariant.ugLight->Finalize();

			// создать uniform-буфер для параметров
			lightVariant.ubLight = device->CreateUniformBuffer(lightVariant.ugLight->GetSize());

			// инициализировать состояние контекста
			ContextState& cs = lightVariant.csOpaque;
			cs = cleanState;
			cs.viewportWidth = screenWidth;
			cs.viewportHeight = screenHeight;
			cs.renderBuffers[0] = rbScreen;
			cs.depthStencilBuffer = dsbDepth;
			cs.uniformBuffers[ugCamera->GetSlot()] = ubCamera;
			cs.uniformBuffers[lightVariant.ugLight->GetSlot()] = lightVariant.ubLight;
			cs.uniformBuffers[ugMaterial->GetSlot()] = ubMaterial;
			cs.uniformBuffers[ugModel->GetSlot()] = ubModel;

			// применить семплер случайной текстуры
			uRandomSampler.Apply(cs);

			// применить семплеры карт теней
			for(int i = 0; i < shadowLightsCount; ++i)
			{
				ShadowLight& shadowLight = lightVariant.shadowLights[i];
				shadowLight.uShadowSampler.SetTexture(dsbShadows[i]->GetTexture());
				shadowLight.uShadowSampler.SetSamplerState(shadowSamplerState);
				shadowLight.uShadowSampler.Apply(cs);
			}
		}

	//** шейдеры материалов

	// генератор шейдеров и компилятор
	ptr<HlslGenerator> shaderGenerator = NEW(HlslGenerator());
	ptr<DxShaderCompiler> shaderCompiler = NEW(DxShaderCompiler());
	// переменные шейдеров
	Temp<float4x4> tmpWorld;
	Temp<float4> tmpWorldPosition;
	Interpolant<float4> tPosition(Semantics::VertexPosition);
	Interpolant<float3> tNormal(Semantics::CustomNormal);
	Interpolant<float2> tTexcoord(Semantics::CustomTexcoord0);
	Interpolant<float3> tWorldPosition(Semantic(Semantics::CustomTexcoord0 + 1));
	Fragment<float4> tTarget(Semantics::TargetColor0);
	// номер instance
	Value<unsigned int> instanceID = NEW(SpecialNode(DataTypes::UInt, Semantics::Instance));
	// вершинный шейдер
	ptr<ShaderSource> vertexShaderSource = shaderGenerator->Generate(Expression((
		tmpWorld = uWorlds[instanceID],
		tmpWorldPosition = mul(aPosition, tmpWorld),
		tPosition = mul(tmpWorldPosition, uViewProj),
		tNormal = mul(aNormal, tmpWorld.Cast<float3x3>()),
		tTexcoord = aTexcoord,
		tWorldPosition = tmpWorldPosition.Swizzle<float3>("xyz")
		)), ShaderTypes::vertex);
	ptr<VertexShader> vertexShader = device->CreateVertexShader(shaderCompiler->Compile(vertexShaderSource));

	// вершинный шейдер для теней
	ptr<ShaderSource> shadowVertexShaderSource = shaderGenerator->Generate(Expression((
		tPosition = mul(mul(aPosition, uWorlds[instanceID]), uViewProj)
		)), ShaderTypes::vertex);
	shadowContextState.vertexShader = device->CreateVertexShader(shaderCompiler->Compile(shadowVertexShaderSource));

	// варианты шейдеров
	for(int basicLightsCount = 0; basicLightsCount <= maxBasicLightsCount; ++basicLightsCount)
		for(int shadowLightsCount = 0; shadowLightsCount <= maxShadowLightsCount; ++shadowLightsCount)
		{
			ShaderKey shaderKey(basicLightsCount, shadowLightsCount, false);

			LightVariant& lightVariant = lightVariants[basicLightsCount][shadowLightsCount];

			// пиксельный шейдер
			Temp<float4> tmpWorldPosition;
			Temp<float3> tmpNormalizedNormal;
			Temp<float3> tmpToCamera;
			Temp<float3> tmpDiffuse;
			Temp<float3> tmpSpecular;
			Temp<float3> tmpColor;
			Expression shader = (
				tPosition,
				tNormal,
				tTexcoord,
				tWorldPosition,
				tmpWorldPosition = newfloat4(tWorldPosition, 1.0f),
				tmpNormalizedNormal = normalize(tNormal),
				tmpToCamera = normalize(uCameraPosition - tWorldPosition),
				//tmpDiffuse = newfloat3(0, 0, 1),
				//tmpSpecular = newfloat3(0, 1, 0),
				//tmpDiffuse = newfloat3(0.5f, 0.5f, 0.5f),
				//tmpSpecular = newfloat3(0.5f, 0.5f, 0.5f),
				tmpDiffuse = uDiffuseSampler.Sample(tTexcoord),
				tmpSpecular = uSpecularSampler.Sample(tTexcoord),
				tmpColor = lightVariant.uAmbientColor * tmpDiffuse
				);

			// учесть все простые источники света
			for(int i = 0; i < basicLightsCount; ++i)
			{
				BasicLight& basicLight = lightVariant.basicLights[i];

				// направление на источник света
				Temp<float3> tmpToLight;
				shader.Assign((shader,
					tmpToLight = normalize(basicLight.uLightPosition - tWorldPosition)
					));
				// диффузная составляющая
				Value<float3> diffuse =
					tmpDiffuse * max(dot(tmpNormalizedNormal, tmpToLight), 0);
				// specular составляющая
				Value<float3> specular =
					tmpSpecular * pow(max(dot(tmpToLight + tmpToCamera, tmpNormalizedNormal), 0), 4.0f);

				// добавка к цвету
				shader.Assign((shader,
					tmpColor = tmpColor + basicLight.uLightColor * (diffuse + specular)
					));
			}

			// общие переменные для источников света с тенями
			Temp<float4> random;
			if(shadowLightsCount)
			{
				shader.Assign((shader,
					random = uRandomSampler.Sample(tPosition.Swizzle<float2>("xy") * Value<float>(1.0f / randomMapSize)) * newfloat4(16, 16, 2.0f / shadowMapSize, 2.0f / shadowMapSize)
					));
			}
			// учесть все источники света с тенями
			for(int i = 0; i < shadowLightsCount; ++i)
			{
				ShadowLight& shadowLight = lightVariant.shadowLights[i];

				// направление на источник света
				Temp<float3> tmpToLight;
				shader.Assign((shader,
					tmpToLight = normalize(shadowLight.uLightPosition - tWorldPosition)
					));
				// диффузная составляющая
				Value<float3> diffuse =
					tmpDiffuse * max(dot(tmpNormalizedNormal, tmpToLight), 0);
				// specular составляющая
				Value<float3> specular =
					tmpSpecular * pow(max(dot(tmpToLight + tmpToCamera, tmpNormalizedNormal), 0), 4.0f);

				// тень
				Temp<float4> shadowCoords;
				Temp<float> shadowMultiplier;
				Temp<float2> shadowCoordsXY;
				Temp<float> originZ;
				Temp<float> lighted;
				shader.Assign((shader,
					shadowCoords = mul(tmpWorldPosition, shadowLight.uLightTransform),
					lighted = shadowCoords.Swizzle<float>("w") > Value<float>(0),
					shadowCoords = shadowCoords / shadowCoords.Swizzle<float>("w"),
					originZ = shadowCoords.Swizzle<float>("z") - Value<float>(0.0005f),
					shadowCoordsXY = newfloat2(
						(shadowCoords.Swizzle<float>("x") + Value<float>(1.0f)) * Value<float>(0.5f),
						(Value<float>(1.0f) - shadowCoords.Swizzle<float>("y")) * Value<float>(0.5f))
						+ random.Swizzle<float2>("zw")
						,
					shadowMultiplier = 0
					));
				static const float poissonDisk[11][2] =
				{
					{ 0.756607, -0.0438077 },
					{ 0.461417, -0.537318 },
					{ 0.549276, 0.44852 },
					{ 0.272938, 0.0256119 },
					{ -0.701909, 0.0956898 },
					{ -0.189804, 0.183371 },
					{ -0.579847, -0.459086 },
					{ -0.427713, 0.61854 },
					{ 0.108073, 0.727706 },
					{ -0.109598, -0.29099 },
					{ -0.0102734, -0.763004 }
				};
				// самый новый способ
				const float m = 1.0f / shadowMapSize;
				Temp<float2> rotate1, rotate2;
				shader.Assign((shader,
					rotate1 = newfloat2(cos(random.Swizzle<float>("x")), sin(random.Swizzle<float>("x"))),
					rotate2 = newfloat2(-rotate1.Swizzle<float>("y"), rotate1.Swizzle<float>("x"))
					));
				for(int j = 0; j < 11; ++j)
				{
					Temp<float2> v;
					shader.Assign((shader,
						v = newfloat2(poissonDisk[j][0] * m, poissonDisk[j][1] * m),
						v = newfloat2(dot(v, rotate1), dot(v, rotate2)),
						shadowMultiplier = shadowMultiplier +
						(shadowLight.uShadowSampler.Sample(shadowCoordsXY + v) > originZ)
						));
				}
				shader.Assign((shader,
					shadowMultiplier = shadowMultiplier * lighted * Value<float>(1.0f / 11)
					));

				// добавка к цвету
				Temp<float3> toLight;
				shader.Assign((shader,
					toLight = shadowLight.uLightPosition - tmpWorldPosition.Swizzle<float3>("xyz"),
					tmpColor = tmpColor + shadowLight.uLightColor /** exp(length(toLight) * Value<float>(-0.05f))*/ * shadowMultiplier * (diffuse + specular)
					));
			}

			// вернуть цвет
			shader.Assign((shader,
				tTarget = newfloat4(tmpColor, 1.0f)
				));

			ptr<ShaderSource> pixelShaderSource = shaderGenerator->Generate(shader, ShaderTypes::pixel);
			ptr<PixelShader> pixelShader = device->CreatePixelShader(shaderCompiler->Compile(pixelShaderSource));

			shaders[shaderKey] = Shader(vertexShader, pixelShader);
		}

	//** шейдеры и геометрия постпроцессинга
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

		ptr<VertexBuffer> vertexBuffer = device->CreateVertexBuffer(MemoryFile::CreateViaCopy(vertices, sizeof(vertices)), layout);
		ptr<IndexBuffer> indexBuffer = device->CreateIndexBuffer(MemoryFile::CreateViaCopy(indices, sizeof(indices)), sizeof(unsigned short));

		ContextState csFilter;
		csFilter.viewportWidth = screenWidth;
		csFilter.viewportHeight = screenHeight;
		csFilter.vertexBuffer = vertexBuffer;
		csFilter.indexBuffer = indexBuffer;
		csFilter.uniformBuffers[ugPostprocess->GetSlot()] = ubPostprocess;

		// атрибуты
		Attribute<float4> aPosition(0);
		Attribute<float2> aTexcoord(1);
		// промежуточные
		Interpolant<float4> iPosition(Semantics::VertexPosition);
		Interpolant<float2> iTexcoord(Semantics::CustomTexcoord0);
		// результат
		Fragment<float4> fTarget(Semantics::TargetColor0);

		// вершинный шейдер - общий для всех постпроцессингов
		ptr<VertexShader> vertexShader = device->CreateVertexShader(shaderCompiler->Compile(shaderGenerator->Generate((
			iPosition = aPosition,
			iTexcoord = aTexcoord
			), ShaderTypes::vertex)));

		csFilter.vertexShader = vertexShader;

		// точки для шейдера
		//const float offsets[] = { -7, -3, -1, 0, 1, 3, 7 };
		const float offsets[] = { -7, -5.9f, -3.2f, -2.1f, -1.1f, -0.5f, 0, 0.5f, 1.1f, 2.1f, 3.2f, 5.9f, 7 };
		const float offsetScaleX = 1.0f / screenWidth, offsetScaleY = 1.0f / screenHeight;
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
			psBloomLimit = device->CreatePixelShader(shaderCompiler->Compile(shaderGenerator->Generate(shader, ShaderTypes::pixel)));
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
			psBloom1 = device->CreatePixelShader(shaderCompiler->Compile(shaderGenerator->Generate(shader, ShaderTypes::pixel)));
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
			psBloom2 = device->CreatePixelShader(shaderCompiler->Compile(shaderGenerator->Generate(shader, ShaderTypes::pixel)));
		}
		// финальный шейдер
		ptr<PixelShader> psFinal;
		{
			Temp<float3> color;
			Temp<float> luminance, relativeLuminance, intensity;
			Expression shader = (
				iPosition,
				iTexcoord,
				color = uScreenSampler.Sample(iTexcoord) + uBloomSourceSampler.Sample(iTexcoord),
				luminance = dot(color, newfloat3(0.2126f, 0.7152f, 0.0722f)),
				relativeLuminance = uLuminanceKey * luminance / uAdaptationLuminance,
				intensity = relativeLuminance * (Value<float>(1) + relativeLuminance / uMaxLuminance) / (Value<float>(1) + relativeLuminance),
				fTarget = newfloat4(color * (intensity / luminance), 1.0f)
				);
			psFinal = device->CreatePixelShader(shaderCompiler->Compile(shaderGenerator->Generate(shader, ShaderTypes::pixel)));
		}

		csBloomLimit = csFilter;
		csBloom1 = csFilter;
		csBloom2 = csFilter;
		csFinal = csFilter;

		ptr<SamplerState> pointSampler = device->CreateSamplerState();
		pointSampler->SetFilter(SamplerState::filterPoint, SamplerState::filterPoint, SamplerState::filterPoint);
		pointSampler->SetWrap(SamplerState::wrapClamp, SamplerState::wrapClamp, SamplerState::wrapClamp);
		ptr<SamplerState> linearSampler = device->CreateSamplerState();
		linearSampler->SetFilter(SamplerState::filterLinear, SamplerState::filterLinear, SamplerState::filterLinear);
		linearSampler->SetWrap(SamplerState::wrapClamp, SamplerState::wrapClamp, SamplerState::wrapClamp);

		uScreenSampler.SetTexture(rbScreen->GetTexture());
		uScreenSampler.SetSamplerState(pointSampler);
		// самый первый проход bloom (из rbScreen в rbBloom2)
		csBloomLimit.renderBuffers[0] = rbBloom2;
		uBloomSourceSampler.SetTexture(rbScreen->GetTexture());
		uBloomSourceSampler.SetSamplerState(linearSampler);
		uBloomSourceSampler.Apply(csBloomLimit);
		csBloomLimit.pixelShader = psBloomLimit;
		// первый проход bloom (из rbBloom1 в rbBloom2)
		csBloom1.renderBuffers[0] = rbBloom2;
		uBloomSourceSampler.SetTexture(rbBloom1->GetTexture());
		uBloomSourceSampler.SetSamplerState(linearSampler);
		uBloomSourceSampler.Apply(csBloom1);
		csBloom1.pixelShader = psBloom1;
		// второй проход bloom (из rbBloom2 в rbBloom1)
		csBloom2.renderBuffers[0] = rbBloom1;
		uBloomSourceSampler.SetTexture(rbBloom2->GetTexture());
		uBloomSourceSampler.SetSamplerState(linearSampler);
		uBloomSourceSampler.Apply(csBloom2);
		csBloom2.pixelShader = psBloom2;
		// финал
		csFinal.renderBuffers[0] = rbBack;
		uBloomSourceSampler.SetTexture(rbBloom1->GetTexture());
		uBloomSourceSampler.SetSamplerState(pointSampler);
		uBloomSourceSampler.Apply(csFinal);
		uScreenSampler.Apply(csFinal);
		csFinal.pixelShader = psFinal;
	}
}

void Painter::BeginFrame()
{
	models.clear();
	lights.clear();
}

void Painter::SetCamera(const float4x4& cameraViewProj, const float3& cameraPosition)
{
	this->cameraViewProj = cameraViewProj;
	this->cameraPosition = cameraPosition;
}

void Painter::AddModel(ptr<Texture> diffuseTexture, ptr<Texture> specularTexture, ptr<VertexBuffer> vertexBuffer, ptr<IndexBuffer> indexBuffer, const float4x4& worldTransform)
{
	models.push_back(Model(diffuseTexture, specularTexture, vertexBuffer, indexBuffer, worldTransform));
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

void Painter::Draw()
{
	// получить количество простых и теневых источников света
	int basicLightsCount = 0;
	int shadowLightsCount = 0;
	for(size_t i = 0; i < lights.size(); ++i)
		++(lights[i].shadow ? shadowLightsCount : basicLightsCount);

	// проверить ограничения
	if(basicLightsCount > maxBasicLightsCount)
		THROW_PRIMARY_EXCEPTION("Too many basic lights");
	if(shadowLightsCount > maxShadowLightsCount)
		THROW_PRIMARY_EXCEPTION("Too many shadow lights");

	// выполнить теневые проходы
	context->GetTargetState() = shadowContextState;
	int shadowPassNumber = 0;
	for(size_t i = 0; i < lights.size(); ++i)
		if(lights[i].shadow)
		{
			// очистить карту теней
			context->ClearDepthStencilBuffer(dsbShadows[shadowPassNumber], 1.0f);
			context->GetTargetState().depthStencilBuffer = dsbShadows[shadowPassNumber];
			shadowPassNumber++;

			// указать трансформацию
			uViewProj.SetValue(lights[i].transform);
			context->SetUniformBufferData(ubCamera, ugCamera->GetData(), ugCamera->GetSize());

			// отсортировать объекты по геометрии
			struct GeometrySorter
			{
				bool operator()(const Model& a, const Model& b) const
				{
					return a.vertexBuffer < b.vertexBuffer || a.vertexBuffer == b.vertexBuffer && a.indexBuffer < b.indexBuffer;
				}
			};
			std::sort(models.begin(), models.end(), GeometrySorter());

			// нарисовать
			ContextState& cs = context->GetTargetState();
			for(size_t j = 0; j < models.size(); )
			{
				// количество рисуемых объектов
				int batchCount;
				for(batchCount = 1;
					batchCount < maxInstancesCount &&
					j + batchCount < models.size() &&
					models[j].vertexBuffer == models[j + batchCount].vertexBuffer &&
					models[j].indexBuffer == models[j + batchCount].indexBuffer;
					++batchCount);

				// установить геометрию
				cs.vertexBuffer = models[j].vertexBuffer;
				cs.indexBuffer = models[j].indexBuffer;
				// установить uniform'ы
				for(int k = 0; k < batchCount; ++k)
					uWorlds.SetValue(j, models[j + k].worldTransform);
				// и залить в GPU
				context->SetUniformBufferData(ubModel, ugModel->GetData(), ugModel->GetSize());

				// нарисовать
				context->DrawInstanced(batchCount);

				j += batchCount;
			}
		}

	// очистить рендербуферы
	float color[4] = { 1, 0, 0, 0 };
	context->ClearRenderBuffer(rbScreen, color);
	context->ClearDepthStencilBuffer(dsbDepth, 1.0f);

	ContextState& cs = context->GetTargetState();

	// установить uniform'ы камеры
	uViewProj.SetValue(cameraViewProj);
	uCameraPosition.SetValue(cameraPosition);
	context->SetUniformBufferData(ubCamera, ugCamera->GetData(), ugCamera->GetSize());

	// установить параметры источников света
	LightVariant& lightVariant = lightVariants[basicLightsCount][shadowLightsCount];
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

	// установить шейдеры
	std::unordered_map<ShaderKey, Shader>::const_iterator it = shaders.find(ShaderKey(basicLightsCount, shadowLightsCount, false/*skinned*/));
	if(it == shaders.end())
		THROW_PRIMARY_EXCEPTION("Shader not compiled");
	const Shader& shader = it->second;
	cs.vertexShader = shader.vertexShader;
	cs.pixelShader = shader.pixelShader;

	// отсортировать объекты по материалу, а затем по геометрии
	struct Sorter
	{
		bool operator()(const Model& a, const Model& b) const
		{
			return
				a.diffuseTexture < b.diffuseTexture || a.diffuseTexture == b.diffuseTexture && (
				a.specularTexture < b.specularTexture || a.specularTexture == b.specularTexture && (
				a.vertexBuffer < b.vertexBuffer || a.vertexBuffer == b.vertexBuffer && (
				a.indexBuffer < b.indexBuffer)));
		}
	};
	std::sort(models.begin(), models.end(), Sorter());

	// нарисовать
	for(size_t i = 0; i < models.size(); )
	{
		// выяснить размер батча по материалу
		int materialBatchCount;
		for(materialBatchCount = 1;
			i + materialBatchCount < models.size() &&
			models[i].diffuseTexture == models[i + materialBatchCount].diffuseTexture &&
			models[i].specularTexture == models[i + materialBatchCount].specularTexture;
			++materialBatchCount);

		// установить параметры материала
		uDiffuseSampler.SetTexture(models[i].diffuseTexture);
		uDiffuseSampler.Apply(cs);
		uSpecularSampler.SetTexture(models[i].specularTexture);
		uSpecularSampler.Apply(cs);

		// цикл по батчам по геометрии
		for(int j = 0; j < materialBatchCount; )
		{
			// выяснить размер батча по геометрии
			int geometryBatchCount;
			for(geometryBatchCount = 1;
				geometryBatchCount < maxInstancesCount &&
				j + geometryBatchCount < materialBatchCount &&
				models[i + j].vertexBuffer == models[i + j + geometryBatchCount].vertexBuffer &&
				models[i + j].indexBuffer == models[i + j + geometryBatchCount].indexBuffer;
				++geometryBatchCount);

			// установить геометрию
			cs.vertexBuffer = models[i + j].vertexBuffer;
			cs.indexBuffer = models[i + j].indexBuffer;

			// установить uniform'ы
			for(int k = 0; k < geometryBatchCount; ++k)
				uWorlds.SetValue(k, models[i + j + k].worldTransform);
			context->SetUniformBufferData(ubModel, ugModel->GetData(), ugModel->GetSize());

			// нарисовать
			context->DrawInstanced(geometryBatchCount);

			j += geometryBatchCount;
		}

		i += materialBatchCount;
	}

	// всё, теперь постпроцессинг
	uBloomLimit.SetValue(1.0f);
	uAdaptationLuminance.SetValue(0.5f);
	uLuminanceKey.SetValue(0.5f);
	uMaxLuminance.SetValue(2.0f);
	context->SetUniformBufferData(ubPostprocess, ugPostprocess->GetData(), ugPostprocess->GetSize());

	float clearColor[] = { 0, 0, 0, 0 };

	const int bloomPassesCount = 9;

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

	cs = csFinal;
	context->ClearRenderBuffer(rbBack, clearColor);
	context->Draw();
}
