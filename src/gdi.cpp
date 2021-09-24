#include "gdi.h"

#include "plugin-macros.generated.h"

#include <windows.h>
#include <gdiplus.h>

static ULONG_PTR gdip_token = 0;

void init_gdi()
{
	const Gdiplus::GdiplusStartupInput gdip_input;
	Gdiplus::GdiplusStartup(&gdip_token, &gdip_input, nullptr);
}

void uninit_gdi()
{
	Gdiplus::GdiplusShutdown(gdip_token);
}

static inline DWORD get_alpha_val(uint32_t opacity)
{
	return ((opacity * 255 / 100) & 0xFF) << 24;
}

static inline DWORD calc_color(uint32_t color, uint32_t opacity)
{
	return color & 0xFFFFFF | get_alpha_val(opacity);
}

static inline uint32_t rgb_to_bgr(uint32_t rgb)
{
	return ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb & 0xFF0000) >> 16);
}

std::vector<uint8_t> render_text(uint32_t width, uint32_t height, const std::wstring& text, uint32_t color, int i)
{
	std::vector<uint8_t> bits(width * height * 4, 0);

	Gdiplus::Bitmap bitmap(width, height, 4 * width, PixelFormat32bppARGB, bits.data());
	Gdiplus::Graphics gr(&bitmap);
	Gdiplus::FontFamily x(L"Arial");
	Gdiplus::Font font(&x, 72);
	Gdiplus::SolidBrush brush = Gdiplus::Color(calc_color(rgb_to_bgr(color), 100));
	if (Gdiplus::Status st = gr.Clear(Gdiplus::Color(0)); st == Gdiplus::Status::Ok)
	{
		Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
		format.SetAlignment((i == 0 ? Gdiplus::StringAlignment::StringAlignmentNear : Gdiplus::StringAlignment::StringAlignmentFar));
		format.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);

		Gdiplus::RectF before;
		if (Gdiplus::Status st = gr.MeasureString(text.c_str(), -1, &font, Gdiplus::PointF(0, 0), &format, &before); st == Gdiplus::Status::Ok && before.Width != 0 && before.Height != 0)
		{
			float scale = min(float(width) / before.Width, float(height) / before.Height);

			if (Gdiplus::Status st = gr.ScaleTransform(scale, scale); st == Gdiplus::Status::Ok)
			{
				if (Gdiplus::Status st = gr.DrawString(text.c_str(), -1, &font, Gdiplus::PointF((i == 0 ? 0 : width) / scale, (height / 2) / scale), &format, &brush); st == Gdiplus::Status::Ok)
				{
					return bits;
				}
				else
				{
					blog(LOG_WARNING, "DrawString failed, with text: %s", text.c_str());
				}
			}
			else
			{
				blog(LOG_WARNING, "ScaleTransform failed, with scale: %f", scale);
			}
		}
		else
		{
			blog(LOG_WARNING, "MeasureString failed, with text: %s", text.c_str());
		}
	}
	else
	{
		blog(LOG_WARNING, "Clear failed");
	}

	return {};
}