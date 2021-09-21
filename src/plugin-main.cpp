#include <obs-module.h>

#include "plugin-macros.generated.h"

#include "lcu.h"
#include "gdi.h"
#include "plugin.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
    init_gdi();
    init_lcu();

    obs_register_source(&si);

    blog(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);

    return true;
}

void obs_module_unload()
{
    uninit_gdi();

    blog(LOG_INFO, "plugin unloaded");
}
