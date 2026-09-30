#pragma once

#include <Math/PDVertex2D.h>

#include <d3d11.h>

#include <cstddef>
#include <vector>

class PDSpriteRenderer
{
public:
	bool initialize(ID3D11Device *device);
	void shutdown();

	void begin(ID3D11DeviceContext *context, int screenWidth, int screenHeight);

	void draw(
		ID3D11ShaderResourceView *texture,
		float x,
		float y,
		float width,
		float height,
		float u0,
		float v0,
		float u1,
		float v1);

	void end();

private:
	static constexpr std::size_t MaxQuads = 4096;

	struct Batch
	{
		ID3D11ShaderResourceView *texture = nullptr;
		UINT firstQuad = 0;
		UINT quadCount = 0;
	};

	void flush();

	ID3D11VertexShader *m_vertexShader = nullptr;
	ID3D11PixelShader *m_pixelShader = nullptr;
	ID3D11InputLayout *m_inputLayout = nullptr;
	ID3D11Buffer *m_vertexBuffer = nullptr;
	ID3D11Buffer *m_indexBuffer = nullptr;
	ID3D11SamplerState *m_sampler = nullptr;
	ID3D11BlendState *m_blendState = nullptr;

	ID3D11DeviceContext *m_context = nullptr;
	int m_screenWidth = 0;
	int m_screenHeight = 0;

	std::vector<PDVertex2D> m_vertices;
	std::vector<Batch> m_batches;
};
