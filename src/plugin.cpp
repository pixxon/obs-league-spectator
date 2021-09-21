#include "plugin.h"

#include "lcu.h"
#include "gdi.h"

#include <util/platform.h>
#include <obs-module.h>
#include <memory>
#include <thread>
#include <mutex>

struct worker
{
	template<class Function, class... Args>
	worker(Function&& f, Args&&... args)
		: task(helper<Function, Args...>, std::ref(end), std::ref(f), std::ref(args)...)
	{
	}

	~worker()
	{
		end.store(true);
		task.join();
	}

private:
	std::atomic<bool> end{ false };
	std::thread task;

	template<class Function, class... Args>
	static void helper(std::atomic<bool>& end, Function&& f, Args&&... args)
	{
		while(!end)
		{
			f(std::forward<Args>(args)...);
		}
	}
};

struct userdata
{
	uint32_t w;
	uint32_t h;
	uint32_t gap;
	std::string path;

	uint32_t colorLeft;
	uint32_t colorRight;

	std::wstring textLeft = L"";
	std::wstring textRight = L"";

	std::mutex m;
};

static inline std::wstring to_wide(const std::string& utf8)
{
	std::wstring text;

	size_t len = os_utf8_to_wcs(utf8.c_str(), 0, nullptr, 0);
	text.resize(len);
	if (len)
		os_utf8_to_wcs(utf8.c_str(), 0, &text[0], len + 1);

	return text;
}

void update(userdata& data)
{
	std::string path;
	{
		std::unique_lock<std::mutex> lock(data.m);
		path = data.path;
	}

	auto m = get_pros(path);

	std::wstring textLeft;
	std::wstring textRight;

	auto s = get_players();
	for(const auto& p : s)
	{
		auto it = m.find(p.name);
		if (it != m.end())
		{
			(p.team == "ORDER" ? textLeft : textRight).append(to_wide(it->second.c_str()) + L" (" + to_wide(p.champion.c_str()) + L")\n");
		}
	}

	{
		std::unique_lock<std::mutex> lock(data.m);
		data.textLeft = textLeft;
		data.textRight = textRight;
	}

	using namespace std::chrono_literals;
	std::this_thread::sleep_for(1000ms);
}

struct foo
{
	gs_texture_t* texLeft = nullptr;
	gs_texture_t* texRight = nullptr;
	obs_source_t* source = nullptr;

	userdata ud;
	worker w;

	foo()
		: w(update, ud)
	{
	}
};


static void missing_file_callback(void*, const char* new_path, void* data)
{
	auto foo_data = reinterpret_cast<foo*>(data);

	obs_source_t *source = foo_data->source;
	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_set_string(settings, "path", new_path);
	obs_source_update(source, settings);
	obs_data_release(settings);
}

gs_texture_t* make_tex(gs_texture_t* tex, uint32_t width, uint32_t height, const std::wstring& text, uint32_t color, int i)
{
	auto e = render_text(width, height, text, color, i);
	const uint8_t* ep = e.data();

	obs_enter_graphics();
	if (tex)
		gs_texture_destroy(tex);
	auto* result_tex = gs_texture_create(width, height, GS_BGRA, 1, &ep, GS_DYNAMIC);
	obs_leave_graphics();
	return result_tex;
}

void asd(foo* data)
{
	auto width = (data->ud.w - data->ud.gap) / 2;

	std::wstring textLeft;
	std::wstring textRight;
	{
		std::unique_lock<std::mutex> lock(data->ud.m);
		textLeft = data->ud.textLeft;
		textRight = data->ud.textRight;
	}

	data->texLeft = make_tex(data->texLeft, width, data->ud.h, textLeft, data->ud.colorLeft, 0);
	data->texRight = make_tex(data->texRight, width, data->ud.h, textRight, data->ud.colorRight, 1);
}

obs_source_info make_source()
{
	obs_source_info si = {};

	si.id = "obs_league_spectator";
	si.type = OBS_SOURCE_TYPE_INPUT;
	si.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;

	si.get_properties = [](void* data)
	{
		auto foo_data = reinterpret_cast<foo*>(data);

		obs_properties_t *props = obs_properties_create();
		obs_properties_add_int(props, "width", "Width", 0, 4096, 1);
		obs_properties_add_int(props, "height", "Height", 0, 4096, 1);
		obs_properties_add_int(props, "gap", "Gap", 0, 4096, 1);
		obs_properties_add_path(props, "path", "Path", OBS_PATH_FILE, " (*.*)", foo_data->ud.path.c_str());
		obs_properties_add_color(props, "colorLeft", "Color Left");
		obs_properties_add_color(props, "colorRight", "Color Right");
		return props;
	};

	si.icon_type = OBS_ICON_TYPE_TEXT;

	si.get_name = [](void*)
	{
		return "League Spectator";
	};

	si.create = [](obs_data_t* settings, obs_source_t* source)
	{
		uint32_t width = (uint32_t)obs_data_get_int(settings, "width");
		uint32_t height = (uint32_t)obs_data_get_int(settings, "height");
		uint32_t gap = (uint32_t)obs_data_get_int(settings, "gap");
		const char* path = obs_data_get_string(settings, "path");
		uint32_t colorLeft = (uint32_t)obs_data_get_int(settings, "colorLeft");
		uint32_t colorRight = (uint32_t)obs_data_get_int(settings, "colorRight");

		auto f = new foo();
		f->ud.w = width;
		f->ud.h = height;
		f->ud.gap = gap;
		f->source = source;
		f->ud.path = path;
		f->ud.colorLeft = colorLeft;
		f->ud.colorRight = colorRight;

		return static_cast<void*>(f);
	};

	si.destroy = [](void *data)
	{
		auto* foo_data = static_cast<foo*>(data);
		delete foo_data;
	};

	si.get_width = [](void *data)
	{
		return reinterpret_cast<foo*>(data)->ud.w;
	};

	si.get_height = [](void *data)
	{
		return reinterpret_cast<foo*>(data)->ud.h;
	};

	si.get_defaults = [](obs_data_t *settings)
	{
		obs_data_set_default_int(settings, "width", 800);
		obs_data_set_default_int(settings, "height", 200);
		obs_data_set_default_int(settings, "gap", 100);
		obs_data_set_default_int(settings, "colorLeft", 0xFF0000);
		obs_data_set_default_int(settings, "colorRight", 0x0000FF);
	};

	si.update = [](void *data, obs_data_t *settings)
	{
		auto foo_data = reinterpret_cast<foo*>(data);

		uint32_t width = (uint32_t)obs_data_get_int(settings, "width");
		uint32_t height = (uint32_t)obs_data_get_int(settings, "height");
		uint32_t gap = (uint32_t)obs_data_get_int(settings, "gap");
		const char* path = obs_data_get_string(settings, "path");
		uint32_t colorLeft = (uint32_t)obs_data_get_int(settings, "colorLeft");
		uint32_t colorRight = (uint32_t)obs_data_get_int(settings, "colorRight");

		foo_data->ud.w = width;
		foo_data->ud.h = height;
		foo_data->ud.gap = gap;
		{
			std::unique_lock<std::mutex> lock(foo_data->ud.m);
			foo_data->ud.path = path;
		}
		foo_data->ud.colorLeft = colorLeft;
		foo_data->ud.colorRight = colorRight;

		asd(foo_data);
	};

	si.video_tick = [](void* data, float)
	{
		auto foo_data = reinterpret_cast<foo*>(data);
		asd(foo_data);
	};

	si.video_render = [](void* data, gs_effect_t* effect)
	{
		auto foo_data = reinterpret_cast<foo*>(data);

		gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

		const bool previous = gs_framebuffer_srgb_enabled();
		gs_enable_framebuffer_srgb(true);

		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);

		auto width = (foo_data->ud.w - foo_data->ud.gap) / 2;

		gs_effect_set_texture_srgb(gs_effect_get_param_by_name(effect, "image"), foo_data->texLeft);
		obs_source_draw(foo_data->texLeft, 0, 0, width, foo_data->ud.h, false);

		gs_effect_set_texture_srgb(gs_effect_get_param_by_name(effect, "image"), foo_data->texRight);
		obs_source_draw(foo_data->texRight, width + foo_data->ud.gap, 0, width, foo_data->ud.h, false);

		gs_technique_end_pass(tech);
		gs_technique_end(tech);

		gs_enable_framebuffer_srgb(previous);
	};
	
	si.missing_files = [](void* data)
	{
		auto foo_data = reinterpret_cast<foo*>(data);

		obs_missing_files_t *files = obs_missing_files_create();

		if (!os_file_exists(foo_data->ud.path.c_str()))
		{
			obs_missing_file_t *file = obs_missing_file_create(foo_data->ud.path.c_str(), missing_file_callback, OBS_MISSING_FILE_SOURCE, foo_data->source, data);
			obs_missing_files_add_file(files, file);
		}

		return files;
	};

	return si;
}

obs_source_info si = make_source();
