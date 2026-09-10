/*
Vertical Stats for OBS
Copyright (C) 2026 Nathan V

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <QMainWindow>

#include "vertical-stats.hpp"

#define DOCK_ID "vertical-stats-dock"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Vertical Stats";
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS Stats panel in a narrow, vertical layout, with Aitum Vertical output stats when present.";
}

static VerticalStats *stats_widget = nullptr;

bool obs_module_load(void)
{
	QMainWindow *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main) {
		obs_log(LOG_WARNING, "no main window; not adding dock");
		return true;
	}

	stats_widget = new VerticalStats(main);
	/* OBS takes ownership of the widget and shows it under Docks. */
	obs_frontend_add_dock_by_id(DOCK_ID, obs_module_text("VerticalStats.DockTitle"), stats_widget);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_dock(DOCK_ID);
	stats_widget = nullptr;
	obs_log(LOG_INFO, "plugin unloaded");
}
