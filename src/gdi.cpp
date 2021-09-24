#include "gdi.h"

#include "plugin-macros.generated.h"

#include <windows.h>
#include <gdiplus.h>

#include <numeric>

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

std::vector<std::wstring> split_string(const std::wstring& str, const std::wstring& delimiter)
{
    std::vector<std::wstring> strings;

    std::wstring::size_type prev = 0;
	for (std::wstring::size_type pos = str.find(delimiter, prev); pos != std::string::npos; pos = str.find(delimiter, prev))
    {
        strings.push_back(str.substr(prev, pos - prev));
        prev = pos + delimiter.size();
    }

    strings.push_back(str.substr(prev));
    return strings;
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
		if (Gdiplus::Status st = gr.MeasureString(text.c_str(), static_cast<int>(text.size()), &font, Gdiplus::PointF(0, 0), &format, &before); st == Gdiplus::Status::Ok && before.Width != 0 && before.Height != 0)
		{
			float width_scale = float(width) / before.Width;
			float height_scale = float(height) / before.Height;

			if ((height_scale * 2) < width_scale && text.find(L'\n') != std::wstring::npos)
			{
				std::vector<std::wstring> lines = split_string(text, L"\n");

				const auto concat = [](std::wstring s0, const std::wstring& s1) { return std::move(s0.append(L"\n").append(s1)); };
				std::wstring leftPart = std::accumulate(lines.begin() + 1, lines.begin() + (lines.size() / 2), lines.front(), concat);
				std::wstring rightPart = std::accumulate(lines.begin() + 1 + (lines.size() / 2), lines.end(), *(lines.begin() + (lines.size() / 2)), concat);
				if (lines.size() % 2 == 0)
				{
					rightPart.append(L"\u00A0");
				}

				Gdiplus::RectF beforeLeft;
				if (Gdiplus::Status st = gr.MeasureString(leftPart.c_str(), static_cast<int>(leftPart.size()), &font, Gdiplus::PointF(0, 0), &format, &beforeLeft); st == Gdiplus::Status::Ok && beforeLeft.Width != 0 && beforeLeft.Height != 0)
				{
					Gdiplus::RectF beforeRight;
					if (Gdiplus::Status st = gr.MeasureString(rightPart.c_str(), static_cast<int>(rightPart.size()), &font, Gdiplus::PointF(0, 0), &format, &beforeRight); st == Gdiplus::Status::Ok && beforeRight.Width != 0 && beforeRight.Height != 0)
					{
						float double_width_scale = min(float(width / 2) / beforeLeft.Width, float(width / 2) / beforeRight.Width);
						float double_height_scale = min(float(height) / beforeLeft.Height, float(height) / beforeRight.Height);
						float double_scale = min(double_width_scale, double_height_scale);

						if (Gdiplus::Status st = gr.ScaleTransform(double_scale, double_scale); st == Gdiplus::Status::Ok)
						{
							if (Gdiplus::Status st = gr.DrawString(leftPart.c_str(), static_cast<int>(leftPart.size()), &font, Gdiplus::PointF((i == 0 ? 0 : width) / double_scale, (height / 2) / double_scale), &format, &brush); st == Gdiplus::Status::Ok)
							{
								if (Gdiplus::Status st = gr.DrawString(rightPart.c_str(), static_cast<int>(rightPart.size()), &font, Gdiplus::PointF((width / 2) / double_scale, (height / 2) / double_scale), &format, &brush); st == Gdiplus::Status::Ok)
								{
									return bits;
								}
								else
								{
									blog(LOG_WARNING, "DrawString failed, with text: %s", rightPart.c_str());
								}
							}
							else
							{
								blog(LOG_WARNING, "DrawString failed, with text: %s", leftPart.c_str());
							}
						}
						else
						{
							blog(LOG_WARNING, "ScaleTransform failed, with scale: %f", double_scale);
						}
					}
				}
			}
			else
			{
				float scale = min(width_scale, height_scale);
				if (Gdiplus::Status st = gr.ScaleTransform(scale, scale); st == Gdiplus::Status::Ok)
				{
					if (Gdiplus::Status st = gr.DrawString(text.c_str(), static_cast<int>(text.size()), &font, Gdiplus::PointF((i == 0 ? 0 : width) / scale, (height / 2) / scale), &format, &brush); st == Gdiplus::Status::Ok)
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