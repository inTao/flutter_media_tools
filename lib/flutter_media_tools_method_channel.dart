import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'flutter_media_tools_platform_interface.dart';

/// An implementation of [FlutterMediaToolsPlatform] that uses method channels.
class MethodChannelFlutterMediaTools extends FlutterMediaToolsPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('flutter_media_tools');

  @override
  Future<String?> getPlatformVersion() async {
    final version = await methodChannel.invokeMethod<String>(
      'getPlatformVersion',
    );
    return version;
  }
}
