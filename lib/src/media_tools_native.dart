import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'native_library.dart';

class ImageInfo {
  final int width;
  final int height;
  final String format;

  const ImageInfo({
    required this.width,
    required this.height,
    required this.format,
  });

  @override
  String toString() => 'ImageInfo(${width}x$height, $format)';
}

class MediaToolsNative {
  MediaToolsNative._();
  static final MediaToolsNative instance = MediaToolsNative._();

  final _lib = NativeLibrary.instance;

  String greet(String name) {
    final namePtr = name.toNativeUtf8();
    try {
      final resultPtr = _lib.greet(namePtr);
      final result = resultPtr.toDartString();
      _lib.freeString(resultPtr);
      return result;
    } finally {
      calloc.free(namePtr);
    }
  }

  ImageInfo imageInfo(Uint8List data) {
    return _withData(data, (ptr, len) {
      final result = _lib.imageInfo(ptr, len);
      return _checkResult(result, (bytes) {
        final parts = String.fromCharCodes(bytes).split('|');
        return ImageInfo(
          width: int.parse(parts[0]),
          height: int.parse(parts[1]),
          format: parts[2],
        );
      });
    });
  }

  Uint8List resizeImage(Uint8List data, int width, int height) {
    return _withData(data, (ptr, len) {
      final result = _lib.resizeImage(ptr, len, width, height);
      return _checkResult(result, (bytes) => bytes);
    });
  }

  Uint8List rotateImage(Uint8List data, double degrees) {
    return _withData(data, (ptr, len) {
      final result = _lib.rotateImage(ptr, len, degrees);
      return _checkResult(result, (bytes) => bytes);
    });
  }

  Uint8List cropImage(Uint8List data, int x, int y, int width, int height) {
    return _withData(data, (ptr, len) {
      final result = _lib.cropImage(ptr, len, x, y, width, height);
      return _checkResult(result, (bytes) => bytes);
    });
  }

  Uint8List blurImage(Uint8List data, double sigma) {
    return _withData(data, (ptr, len) {
      final result = _lib.blurImage(ptr, len, sigma);
      return _checkResult(result, (bytes) => bytes);
    });
  }

  Uint8List grayscaleImage(Uint8List data) {
    return _withData(data, (ptr, len) {
      final result = _lib.grayscaleImage(ptr, len);
      return _checkResult(result, (bytes) => bytes);
    });
  }

  Uint8List convertFormat(Uint8List data, String format) {
    return _withData(data, (ptr, len) {
      final formatPtr = format.toNativeUtf8();
      try {
        final result = _lib.convertFormat(ptr, len, formatPtr);
        return _checkResult(result, (bytes) => bytes);
      } finally {
        calloc.free(formatPtr);
      }
    });
  }

  T _withData<T>(Uint8List data, T Function(Pointer<Uint8>, int) fn) {
    final ptr = calloc<Uint8>(data.length);
    try {
      ptr.asTypedList(data.length).setAll(0, data);
      return fn(ptr, data.length);
    } finally {
      calloc.free(ptr);
    }
  }

  T _checkResult<T>(
    NativeImageResult result,
    T Function(Uint8List) onSuccess,
  ) {
    try {
      if (result.error != nullptr) {
        throw Exception(result.error.toDartString());
      }
      if (result.data == nullptr) {
        throw Exception('Native function returned null data');
      }
      final bytes = result.data.asTypedList(result.len);
      return onSuccess(Uint8List.fromList(bytes));
    } finally {
      _lib.freeResult(result);
    }
  }
}
