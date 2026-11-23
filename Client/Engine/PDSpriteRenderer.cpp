#include <Engine/PDSpriteRenderer.h>

#include <d3dcompiler.h>

#include <cstdint>
#include <cstring>
#include <string_view>

static std::string_view const VertexShaderSource = R"(
		struct VSInput { float2 pos : POSITION; float2 uv : TEXCOORD0; };
		struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

		PSInput main(VSInput input)
		{
			PSInput output;
			output.pos = float4(input.pos, 0.0f, 1.0f);
			output.uv = input.uv;
			return output;
		}
	)";

static std::string_view const PixelShaderSource = R"(
		Texture2D spriteTexture : register(t0);
		SamplerState spriteSampler : register(s0);

		float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
		{
			return spriteTexture.Sample(spriteSampler, uv);
		}
	)";

static ID3DBlob* compileShader(std::string_view source, char const* target)
{
	ID3DBlob* code = nullptr;
	ID3DBlob* errors = nullptr;

	HRESULT const hr = D3DCompile(
		source.data(),
		source.size(),
		nullptr,
		nullptr,
		nullptr,
		"main",
		target,
		0,
		0,
		&code,
		&errors);

	if (errors != nullptr)
	{
		errors->Release();
	}

	if (FAILED(hr))
	{
		return nullptr;
	}

	return code;
}

bool PDSpriteRenderer::initialize(ID3D11Device* device)
{
	ID3DBlob* vertexBlob = compileShader(VertexShaderSource, "vs_4_0");

	if (vertexBlob == nullptr)
	{
		return false;
	}

	ID3DBlob* pixelBlob = compileShader(PixelShaderSource, "ps_4_0");

	if (pixelBlob == nullptr)
	{
		vertexBlob->Release();

		return false;
	}

	device->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), nullptr, &m_vertexShader);
	device->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(), nullptr, &m_pixelShader);

	D3D11_INPUT_ELEMENT_DESC const layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	device->CreateInputLayout(layout, 2, vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), &m_inputLayout);

	vertexBlob->Release();
	pixelBlob->Release();

	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexBufferDesc.ByteWidth = (UINT)(sizeof(PDVertex2D) * 4 * MaxQuads);
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	device->CreateBuffer(&vertexBufferDesc, nullptr, &m_vertexBuffer);

	std::vector<std::uint16_t> indices;
	indices.reserve(MaxQuads * 6);

	for (std::size_t i = 0; i < MaxQuads; i += 1)
	{
		std::uint16_t const first = (std::uint16_t)(i * 4);
		indices.push_back(first);
		indices.push_back((std::uint16_t)(first + 1));
		indices.push_back((std::uint16_t)(first + 2));
		indices.push_back((std::uint16_t)(first + 2));
		indices.push_back((std::uint16_t)(first + 1));
		indices.push_back((std::uint16_t)(first + 3));
	}

	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexBufferDesc.ByteWidth = (UINT)(sizeof(std::uint16_t) * indices.size());
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices.data();
	device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);

	m_vertices.reserve(MaxQuads * 4);

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	device->CreateSamplerState(&samplerDesc, &m_sampler);

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	device->CreateBlendState(&blendDesc, &m_blendState);

	if (m_vertexShader == nullptr or m_pixelShader == nullptr or m_inputLayout == nullptr)
	{
		shutdown();

		return false;
	}

	if (m_vertexBuffer == nullptr or m_indexBuffer == nullptr or m_sampler == nullptr or m_blendState == nullptr)
	{
		shutdown();

		return false;
	}

	return true;
}

void PDSpriteRenderer::shutdown()
{
	if (m_blendState != nullptr)
	{
		m_blendState->Release();
		m_blendState = nullptr;
	}

	if (m_sampler != nullptr)
	{
		m_sampler->Release();
		m_sampler = nullptr;
	}

	if (m_indexBuffer != nullptr)
	{
		m_indexBuffer->Release();
		m_indexBuffer = nullptr;
	}

	if (m_vertexBuffer != nullptr)
	{
		m_vertexBuffer->Release();
		m_vertexBuffer = nullptr;
	}

	if (m_inputLayout != nullptr)
	{
		m_inputLayout->Release();
		m_inputLayout = nullptr;
	}

	if (m_pixelShader != nullptr)
	{
		m_pixelShader->Release();
		m_pixelShader = nullptr;
	}

	if (m_vertexShader != nullptr)
	{
		m_vertexShader->Release();
		m_vertexShader = nullptr;
	}
}

void PDSpriteRenderer::begin(ID3D11DeviceContext* context, int screenWidth, int screenHeight)
{
	m_context = context;
	m_screenWidth = screenWidth;
	m_screenHeight = screenHeight;
	m_vertices.clear();
	m_batches.clear();

	constexpr float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	context->OMSetBlendState(m_blendState, blendFactor, D3D11_DEFAULT_SAMPLE_MASK);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R16_UINT, 0);
	context->IASetInputLayout(m_inputLayout);
	context->VSSetShader(m_vertexShader, nullptr, 0);
	context->PSSetShader(m_pixelShader, nullptr, 0);
	context->PSSetSamplers(0, 1, &m_sampler);

	UINT const stride = sizeof(PDVertex2D);
	UINT const offset = 0;
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
}

void PDSpriteRenderer::draw(
	ID3D11ShaderResourceView* texture,
	float x,
	float y,
	float width,
	float height,
	float u0,
	float v0,
	float u1,
	float v1)
{
	if (m_context == nullptr or texture == nullptr or m_screenWidth <= 0 or m_screenHeight <= 0)
	{
		return;
	}

	float const left = x / (float)m_screenWidth * 2.0f - 1.0f;
	float const right = (x + width) / (float)m_screenWidth * 2.0f - 1.0f;
	float const top = 1.0f - y / (float)m_screenHeight * 2.0f;
	float const bottom = 1.0f - (y + height) / (float)m_screenHeight * 2.0f;

	if (m_vertices.size() >= MaxQuads * 4)
	{
		flush();
	}

	if (m_batches.empty() or m_batches.back().texture != texture)
	{
		Batch batch;
		batch.texture = texture;
		batch.firstQuad = (UINT)(m_vertices.size() / 4);
		m_batches.push_back(batch);
	}

	m_batches.back().quadCount += 1;
	m_vertices.push_back({left, top, u0, v0});
	m_vertices.push_back({right, top, u1, v0});
	m_vertices.push_back({left, bottom, u0, v1});
	m_vertices.push_back({right, bottom, u1, v1});
}

void PDSpriteRenderer::end()
{
	flush();
	m_context = nullptr;
}

void PDSpriteRenderer::flush()
{
	if (m_context == nullptr or m_vertices.empty())
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE mapped;

	if (m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped) == S_OK)
	{
		std::memcpy(mapped.pData, m_vertices.data(), m_vertices.size() * sizeof(PDVertex2D));
		m_context->Unmap(m_vertexBuffer, 0);

		for (Batch const& batch : m_batches)
		{
			m_context->PSSetShaderResources(0, 1, &batch.texture);
			m_context->DrawIndexed(batch.quadCount * 6, batch.firstQuad * 6, 0);
		}
	}

	m_vertices.clear();
	m_batches.clear();
}
