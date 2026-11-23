#include <Engine/PDTextCache.h>

#include <Core/PDString.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

static HRESULT drawLabel(ID2D1Factory* factory, ID3D11Texture2D* target, IDWriteTextLayout* layout, int width, int height)
{
	IDXGISurface* surface = nullptr;
	ID2D1RenderTarget* renderTarget = nullptr;

	if (SUCCEEDED(target->QueryInterface(IID_PPV_ARGS(&surface))))
	{
		D2D1_RENDER_TARGET_PROPERTIES const properties = D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

		factory->CreateDxgiSurfaceRenderTarget(surface, properties, &renderTarget);
		surface->Release();
	}

	if (renderTarget == nullptr)
	{
		return E_FAIL;
	}

	ID2D1SolidColorBrush* pill = nullptr;
	ID2D1SolidColorBrush* ink = nullptr;
	renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.09f, 0.09f, 0.11f, 0.72f), &pill);
	renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &ink);

	renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	renderTarget->BeginDraw();
	renderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

	if (pill != nullptr and ink != nullptr)
	{
		D2D1_RECT_F const rect = D2D1::RectF(0.0f, 0.0f, (float)width, (float)height);

		renderTarget->FillRoundedRectangle(D2D1::RoundedRect(rect, 7.0f, 7.0f), pill);
		renderTarget->DrawTextLayout(D2D1::Point2F(7.0f, 3.0f), layout, ink);
	}

	HRESULT const hr = renderTarget->EndDraw();

	if (ink != nullptr)
	{
		ink->Release();
	}

	if (pill != nullptr)
	{
		pill->Release();
	}

	renderTarget->Release();

	return hr;
}

static std::vector<std::uint8_t> readPixels(ID3D11Device* device, ID3D11Texture2D* target, int width, int height)
{
	D3D11_TEXTURE2D_DESC stagingDesc = {};
	target->GetDesc(&stagingDesc);
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	ID3D11Texture2D* staging = nullptr;
	device->CreateTexture2D(&stagingDesc, nullptr, &staging);

	ID3D11DeviceContext* context = nullptr;
	device->GetImmediateContext(&context);

	std::vector<std::uint8_t> pixels;
	D3D11_MAPPED_SUBRESOURCE mapped = {};

	if (staging != nullptr and context != nullptr)
	{
		context->CopyResource(staging, target);

		if (SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)))
		{
			pixels.resize((std::size_t)width * (std::size_t)height * 4);

			for (int i = 0; i < height; i += 1)
			{
				std::uint8_t const* row = static_cast<std::uint8_t const*>(mapped.pData) + (std::size_t)i * mapped.RowPitch;

				for (int j = 0; j < width; j += 1)
				{
					std::uint8_t const* source = row + (std::size_t)j * 4;
					std::uint8_t* destination = pixels.data() + ((std::size_t)i * (std::size_t)width + (std::size_t)j) * 4;
					int const alpha = source[3];

					for (int k = 0; k < 3; k += 1)
					{
						destination[k] = alpha == 0 ? 0 : (std::uint8_t)std::min(255, source[2 - k] * 255 / alpha);
					}

					destination[3] = (std::uint8_t)alpha;
				}
			}

			context->Unmap(staging, 0);
		}
	}

	if (context != nullptr)
	{
		context->Release();
	}

	if (staging != nullptr)
	{
		staging->Release();
	}

	return pixels;
}

static std::vector<std::uint8_t> rasterize(
	ID3D11Device* device,
	ID2D1Factory* factory,
	IDWriteTextLayout* layout,
	int width,
	int height)
{
	D3D11_TEXTURE2D_DESC targetDesc = {};
	targetDesc.Width = (UINT)width;
	targetDesc.Height = (UINT)height;
	targetDesc.MipLevels = 1;
	targetDesc.ArraySize = 1;
	targetDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	targetDesc.SampleDesc.Count = 1;
	targetDesc.Usage = D3D11_USAGE_DEFAULT;
	targetDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

	ID3D11Texture2D* target = nullptr;
	device->CreateTexture2D(&targetDesc, nullptr, &target);

	if (target == nullptr)
	{
		return std::vector<std::uint8_t>();
	}

	std::vector<std::uint8_t> pixels;

	if (SUCCEEDED(drawLabel(factory, target, layout, width, height)))
	{
		pixels = readPixels(device, target, width, height);
	}

	target->Release();

	return pixels;
}

PDTextCache::~PDTextCache()
{
	if (m_textFormat != nullptr)
	{
		m_textFormat->Release();
	}

	if (m_writeFactory != nullptr)
	{
		m_writeFactory->Release();
	}

	if (m_d2dFactory != nullptr)
	{
		m_d2dFactory->Release();
	}
}

void PDTextCache::initialize(ID3D11Device* device)
{
	m_device = device;

	if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory)))
	{
		return;
	}

	HRESULT const hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&m_writeFactory));

	if (FAILED(hr))
	{
		return;
	}

	m_writeFactory->CreateTextFormat(
		L"Segoe UI",
		nullptr,
		DWRITE_FONT_WEIGHT_SEMI_BOLD,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		14.0f,
		L"",
		&m_textFormat);
}

PDTexture const* PDTextCache::texture(std::string const& text)
{
	std::unordered_map<std::string, PDTexture>::iterator const existing = m_textures.find(text);

	if (existing != m_textures.end())
	{
		return &existing->second;
	}

	if (m_device == nullptr or m_textFormat == nullptr or text.empty())
	{
		return nullptr;
	}

	std::wstring const wide = toWide(text);
	IDWriteTextLayout* layout = nullptr;

	HRESULT const hr = m_writeFactory->CreateTextLayout(
		wide.c_str(),
		(UINT32)wide.size(),
		m_textFormat,
		480.0f,
		60.0f,
		&layout);

	if (FAILED(hr))
	{
		return nullptr;
	}

	DWRITE_TEXT_METRICS metrics = {};
	layout->GetMetrics(&metrics);

	int const width = (int)std::ceil(metrics.width + 7.0f * 2.0f);
	int const height = (int)std::ceil(metrics.height + 3.0f * 2.0f);
	std::vector<std::uint8_t> const pixels = rasterize(m_device, m_d2dFactory, layout, width, height);
	layout->Release();

	if (pixels.empty())
	{
		return nullptr;
	}

	return &m_textures.emplace(text, PDTexture(m_device, pixels.data(), width, height)).first->second;
}
