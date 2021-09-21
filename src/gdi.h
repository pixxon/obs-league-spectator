#pragma once

#include <vector>
#include <string>

std::vector<uint8_t> render_text(uint32_t width, uint32_t height, const std::wstring& text, uint32_t color, int i);

void init_gdi();
void uninit_gdi();
