#ifndef FLUTTER_PLUGIN_FLUTTER_MEDIA_TOOLS_PLUGIN_H_
#define FLUTTER_PLUGIN_FLUTTER_MEDIA_TOOLS_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

namespace flutter_media_tools {

class FlutterMediaToolsPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  FlutterMediaToolsPlugin();

  virtual ~FlutterMediaToolsPlugin();

  // Disallow copy and assign.
  FlutterMediaToolsPlugin(const FlutterMediaToolsPlugin&) = delete;
  FlutterMediaToolsPlugin& operator=(const FlutterMediaToolsPlugin&) = delete;

  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

}  // namespace flutter_media_tools

#endif  // FLUTTER_PLUGIN_FLUTTER_MEDIA_TOOLS_PLUGIN_H_
