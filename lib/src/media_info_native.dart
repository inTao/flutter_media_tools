import 'dart:convert';
import 'dart:ffi' as ffi;
import 'dart:isolate';

import 'package:ffi/ffi.dart';

import '../flutter_media_tools_bindings_generated.dart' as bindings;
import 'media_info.dart';

/// Reads structured information from a single local media file.
///
/// When [T] is a concrete media info subtype, the returned result is checked
/// after probing and an [UnsupportedMediaFileException] is thrown if the file is
/// a different media kind.
Future<T> getMediaInfo<T extends BaseMediaInfo>(String path) {
  if (path.trim().isEmpty) {
    throw MediaFileNotFoundException(path, 'Path must not be empty.');
  }
  final Uri? uri = Uri.tryParse(path);
  if (uri != null && uri.hasScheme && uri.scheme != 'file') {
    throw UnsupportedMediaFileException(
      path,
      'Only local file paths are supported.',
    );
  }
  return Isolate.run(() => _getMediaInfoSync(path)).then(_mediaInfoAs<T>);
}

BaseMediaInfo _getMediaInfoSync(String path) {
  final ffi.Pointer<Utf8> nativePath = path.toNativeUtf8();
  ffi.Pointer<ffi.Char> result = ffi.nullptr;
  try {
    result = bindings.fmt_probe_media_file(nativePath.cast<ffi.Char>());
    if (result == ffi.nullptr) {
      throw MediaProbeException(path, 'Native probe returned no result.');
    }
    final String payload = result.cast<Utf8>().toDartString();
    final Object? decoded = jsonDecode(payload);
    if (decoded is! Map<String, Object?>) {
      throw MediaProbeException(path, 'Native probe returned invalid JSON.');
    }
    if (decoded['ok'] == true) {
      return _mediaInfoFromJson(decoded);
    }
    throw _exceptionFromJson(path, decoded);
  } on MediaInfoException {
    rethrow;
  } on FormatException catch (error) {
    throw MediaProbeException(path, 'Native probe JSON error: $error');
  } finally {
    if (result != ffi.nullptr) {
      bindings.fmt_free_string(result);
    }
    malloc.free(nativePath);
  }
}

BaseMediaInfo _mediaInfoFromJson(Map<String, Object?> json) {
  final String path = _string(json, 'path');
  final int fileSize = _int(json, 'fileSize');
  final Map<String, String> metadata = _stringMap(json['metadata']);

  return switch (_string(json, 'kind')) {
    'image' => ImageMediaInfo(
      path: path,
      fileSize: fileSize,
      metadata: metadata,
      format: _imageFormat(_string(json, 'format')),
      width: _int(json, 'width'),
      height: _int(json, 'height'),
      codec: _optionalString(json, 'codec'),
      bitrate: _optionalInt(json, 'bitrate'),
      exif: _stringMap(json['exif']),
    ),
    'video' => VideoMediaInfo(
      path: path,
      fileSize: fileSize,
      metadata: metadata,
      format: _videoFormat(_string(json, 'format')),
      width: _int(json, 'width'),
      height: _int(json, 'height'),
      duration: Duration(microseconds: _int(json, 'durationMicros')),
      frameRate: _optionalDouble(json, 'frameRate'),
      codec: _optionalString(json, 'codec'),
      bitrate: _optionalInt(json, 'bitrate'),
    ),
    'audio' => AudioMediaInfo(
      path: path,
      fileSize: fileSize,
      metadata: metadata,
      format: _audioFormat(_string(json, 'format')),
      duration: Duration(microseconds: _int(json, 'durationMicros')),
      sampleRate: _optionalInt(json, 'sampleRate'),
      channels: _optionalInt(json, 'channels'),
      codec: _optionalString(json, 'codec'),
      bitrate: _optionalInt(json, 'bitrate'),
    ),
    final String kind => throw MediaProbeException(
      path,
      'Unsupported native media kind: $kind.',
    ),
  };
}

T _mediaInfoAs<T extends BaseMediaInfo>(BaseMediaInfo info) {
  if (info is T) {
    return info;
  }
  throw UnsupportedMediaFileException(
    info.path,
    'Expected media info of type $T, but native probe returned '
    '${info.runtimeType}.',
  );
}

MediaInfoException _exceptionFromJson(
  String fallbackPath,
  Map<String, Object?> json,
) {
  final String path = _optionalString(json, 'path') ?? fallbackPath;
  final String message =
      _optionalString(json, 'message') ?? 'Native media probe failed.';
  return switch (_optionalString(json, 'code')) {
    'not_found' || 'not_readable' => MediaFileNotFoundException(path, message),
    'unsupported' => UnsupportedMediaFileException(path, message),
    _ => MediaProbeException(path, message),
  };
}

String _string(Map<String, Object?> json, String key) {
  final Object? value = json[key];
  if (value is String) {
    return value;
  }
  throw MediaProbeException('', 'Expected string field `$key`.');
}

String? _optionalString(Map<String, Object?> json, String key) {
  final Object? value = json[key];
  if (value == null) {
    return null;
  }
  if (value is String) {
    return value;
  }
  throw MediaProbeException('', 'Expected string field `$key`.');
}

int _int(Map<String, Object?> json, String key) {
  final Object? value = json[key];
  if (value is int) {
    return value;
  }
  throw MediaProbeException('', 'Expected integer field `$key`.');
}

int? _optionalInt(Map<String, Object?> json, String key) {
  final Object? value = json[key];
  if (value == null) {
    return null;
  }
  if (value is int) {
    return value;
  }
  throw MediaProbeException('', 'Expected integer field `$key`.');
}

double? _optionalDouble(Map<String, Object?> json, String key) {
  final Object? value = json[key];
  if (value == null) {
    return null;
  }
  if (value is num) {
    return value.toDouble();
  }
  throw MediaProbeException('', 'Expected numeric field `$key`.');
}

Map<String, String> _stringMap(Object? value) {
  if (value == null) {
    return const <String, String>{};
  }
  if (value is! Map<String, Object?>) {
    throw MediaProbeException('', 'Expected string map.');
  }
  return Map<String, String>.unmodifiable(
    value.map<String, String>((String key, Object? value) {
      if (value is! String) {
        throw MediaProbeException('', 'Expected string map value for `$key`.');
      }
      return MapEntry<String, String>(key, value);
    }),
  );
}

ImageFormat _imageFormat(String value) => ImageFormat.values.firstWhere(
  (ImageFormat format) => format.name == value,
  orElse: () => ImageFormat.unknown,
);

VideoFormat _videoFormat(String value) => VideoFormat.values.firstWhere(
  (VideoFormat format) => format.name == value,
  orElse: () => VideoFormat.unknown,
);

AudioFormat _audioFormat(String value) => AudioFormat.values.firstWhere(
  (AudioFormat format) => format.name == value,
  orElse: () => AudioFormat.unknown,
);
