#include "include/flutter_media_tools/flutter_media_tools_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "flutter_media_tools_plugin.h"

void FlutterMediaToolsPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  flutter_media_tools::FlutterMediaToolsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
