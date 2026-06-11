import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';

typedef NativeGreet = Pointer<Utf8> Function(Pointer<Utf8>);
typedef DartGreet = Pointer<Utf8> Function(Pointer<Utf8>);

typedef NativeFreeString = Void Function(Pointer<Utf8>);
typedef DartFreeString = void Function(Pointer<Utf8>);

typedef NativeFreeResult = Void Function(NativeImageResult);
typedef DartFreeResult = void Function(NativeImageResult);

typedef NativeImageProcess = NativeImageResult Function(
    Pointer<Uint8>, IntPtr, Uint32, Uint32);
typedef DartImageProcess = NativeImageResult Function(
    Pointer<Uint8>, int, int, int);

typedef NativeImageProcessSimple = NativeImageResult Function(
    Pointer<Uint8>, IntPtr);
typedef DartImageProcessSimple = NativeImageResult Function(
    Pointer<Uint8>, int);

typedef NativeImageProcessF32 = NativeImageResult Function(
    Pointer<Uint8>, IntPtr, Float);
typedef DartImageProcessF32 = NativeImageResult Function(
    Pointer<Uint8>, int, double);

typedef NativeConvertFormat = NativeImageResult Function(
    Pointer<Uint8>, IntPtr, Pointer<Utf8>);
typedef DartConvertFormat = NativeImageResult Function(
    Pointer<Uint8>, int, Pointer<Utf8>);

typedef NativeCropImage = NativeImageResult Function(
    Pointer<Uint8>, IntPtr, Uint32, Uint32, Uint32, Uint32);
typedef DartCropImage = NativeImageResult Function(
    Pointer<Uint8>, int, int, int, int, int);

final class NativeImageResult extends Struct {
  external Pointer<Uint8> data;
  @IntPtr()
  external int len;
  external Pointer<Utf8> error;
}

DynamicLibrary _openLibrary() {
  if (Platform.isMacOS) {
    return DynamicLibrary.open('libnative.dylib');
  } else if (Platform.isIOS) {
    return DynamicLibrary.process();
  } else if (Platform.isAndroid || Platform.isLinux) {
    return DynamicLibrary.open('libnative.so');
  } else if (Platform.isWindows) {
    return DynamicLibrary.open('native.dll');
  }
  throw UnsupportedError('Platform ${Platform.operatingSystem} not supported');
}

class NativeLibrary {
  NativeLibrary._();

  static final NativeLibrary instance = NativeLibrary._();
  static DynamicLibrary? _lib;

  DynamicLibrary get lib {
    _lib ??= _openLibrary();
    return _lib!;
  }

  late final DartGreet greet = lib
      .lookupFunction<NativeGreet, DartGreet>('native_greet');

  late final DartFreeString freeString = lib
      .lookupFunction<NativeFreeString, DartFreeString>('native_free_string');

  late final DartFreeResult freeResult = lib
      .lookupFunction<NativeFreeResult, DartFreeResult>('native_free_result');

  late final DartImageProcessSimple imageInfo = lib.lookupFunction<
      NativeImageProcessSimple,
      DartImageProcessSimple>('native_image_info');

  late final DartImageProcess resizeImage = lib.lookupFunction<
      NativeImageProcess,
      DartImageProcess>('native_resize_image');

  late final DartImageProcessF32 rotateImage = lib.lookupFunction<
      NativeImageProcessF32,
      DartImageProcessF32>('native_rotate_image');

  late final DartCropImage cropImage = lib.lookupFunction<
      NativeCropImage,
      DartCropImage>('native_crop_image');

  late final DartImageProcessF32 blurImage = lib.lookupFunction<
      NativeImageProcessF32,
      DartImageProcessF32>('native_blur_image');

  late final DartImageProcessSimple grayscaleImage = lib.lookupFunction<
      NativeImageProcessSimple,
      DartImageProcessSimple>('native_grayscale_image');

  late final DartConvertFormat convertFormat = lib.lookupFunction<
      NativeConvertFormat,
      DartConvertFormat>('native_convert_format');
}
