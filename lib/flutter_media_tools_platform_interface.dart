import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'flutter_media_tools_method_channel.dart';

abstract class FlutterMediaToolsPlatform extends PlatformInterface {
  /// Constructs a FlutterMediaToolsPlatform.
  FlutterMediaToolsPlatform() : super(token: _token);

  static final Object _token = Object();

  static FlutterMediaToolsPlatform _instance = MethodChannelFlutterMediaTools();

  /// The default instance of [FlutterMediaToolsPlatform] to use.
  ///
  /// Defaults to [MethodChannelFlutterMediaTools].
  static FlutterMediaToolsPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [FlutterMediaToolsPlatform] when
  /// they register themselves.
  static set instance(FlutterMediaToolsPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<String?> getPlatformVersion() {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }
}
