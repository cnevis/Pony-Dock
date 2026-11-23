#include <Engine/PDTexture.h>

#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <utility>

static ID3D11ShaderResourceView* upload(ID3D11Device* device, std::uint8_t const* pixels, int width, int height)
{
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = (UINT)width;
	textureDesc.Height = (UINT)height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = pixels;
	data.SysMemPitch = (UINT)width * 4;

	ID3D11Texture2D* texture = nullptr;
	device->CreateTexture2D(&textureDesc, &data, &texture);

	if (texture == nullptr)
	{
		return nullptr;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
	viewDesc.Format = textureDesc.Format;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipLevels = 1;

	ID3D11ShaderResourceView* view = nullptr;
	device->CreateShaderResourceView(texture, &viewDesc, &view);
	texture->Release();

	return view;
}

PDTexture::PDTexture(ID3D11Device* device, std::string const& path)
{
	int channels = 0;
	std::uint8_t* pixels = stbi_load(path.c_str(), &m_width, &m_height, &channels, STBI_rgb_alpha);

	if (pixels == nullptr)
	{
		return;
	}

	m_view = upload(device, pixels, m_width, m_height);
	stbi_image_free(pixels);

	if (m_view == nullptr)
	{
		m_width = 0;
		m_height = 0;
	}
}

PDTexture::PDTexture(ID3D11Device* device, std::uint8_t const* pixels, int width, int height)
	: m_width(width),
	  m_height(height)
{
	m_view = upload(device, pixels, width, height);

	if (m_view == nullptr)
	{
		m_width = 0;
		m_height = 0;
	}
}

PDTexture::~PDTexture()
{
	release();
}

PDTexture::PDTexture(PDTexture&& other) noexcept
	: m_view(std::exchange(other.m_view, nullptr)),
	  m_width(std::exchange(other.m_width, 0)),
	  m_height(std::exchange(other.m_height, 0))
{
}

PDTexture& PDTexture::operator=(PDTexture&& other) noexcept
{
	if (this != &other)
	{
		release();

		m_view = std::exchange(other.m_view, nullptr);
		m_width = std::exchange(other.m_width, 0);
		m_height = std::exchange(other.m_height, 0);
	}

	return *this;
}

void PDTexture::release()
{
	if (m_view != nullptr)
	{
		m_view->Release();
		m_view = nullptr;
	}
}
