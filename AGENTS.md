# AGENTS.md — flutter_media_tools

## What this is

A Flutter plugin package with a Rust core for media processing (image operations).
Architecture: **Flutter (Dart) → dart:ffi → Rust cdylib**.

The Rust library (`native/`) handles all heavy processing. Dart calls it via
C-compatible FFI functions. No MethodChannel for native platforms.

## Project structure

```
native/                 # Rust library crate
  src/api/mod.rs        # High-level Rust API (image processing functions)
  src/ffi.rs            # C-compatible FFI wrappers (extern "C")
  build.sh              # Cross-platform Rust compilation script
lib/
  flutter_media_tools.dart          # Public API exports
  src/native_library.dart           # DynamicLibrary loading + FFI type defs
  src/media_tools_native.dart       # Dart API wrapping FFI calls
  flutter_media_tools_platform_interface.dart  # Legacy platform interface (unused)
  flutter_media_tools_method_channel.dart      # Legacy method channel (unused)
example/                # Example Flutter app
```

## Key commands

```bash
# Build Rust for macOS (most common during development)
./native/build.sh macos

# Build Rust for all platforms
./native/build.sh all

# Build Rust for specific platform
./native/build.sh {macos|ios|linux|windows|android}

# Verify Dart code
flutter analyze

# Run example app
cd example && flutter run -d macos
```

## Build requirements

- Rust toolchain (cargo, rustc) must be installed
- For iOS: `rustup target add aarch64-apple-ios`
- For Android: NDK + multiple Rust targets (build.sh handles this)
- **Run `./native/build.sh <platform>` BEFORE `flutter build/run`**
  The Rust dylib must exist at `macos/Libraries/libnative.dylib` etc. before
  the Flutter build picks it up via podspec/CMakeLists.

## FFI conventions

All FFI functions follow `native_*` naming in Rust, `Dart*` typedefs in Dart.
Data flows: Dart allocates → copies bytes to native memory → calls Rust →
reads result → frees. `NativeResult` struct carries data pointer + length + error.
Rust owns result memory; Dart must call `native_free_result` after reading.

## Platform build integration

- **macOS/iOS**: podspec `vendored_libraries` bundles the compiled Rust lib
- **Linux/Windows**: CMakeLists `bundled_libraries` includes the .so/.dll
- **Android**: Rust .so placed in `jniLibs/{abi}/` — Gradle packages automatically

## Adding new Rust functions

1. Add Rust function in `native/src/api/mod.rs`
2. Add FFI wrapper in `native/src/ffi.rs` (extern "C", NativeResult return)
3. Add typedef + lookup in `lib/src/native_library.dart`
4. Add Dart wrapper in `lib/src/media_tools_native.dart`
5. Export from `lib/flutter_media_tools.dart` if public API
6. Run `./native/build.sh macos` and `flutter analyze`

## Notes

- The `ffi` package (`package:ffi`) provides `Utf8` extensions (`toNativeUtf8`,
  `toDartString`). Always import `package:ffi/ffi.dart` alongside `dart:ffi`.
- The `image` crate (v0.25) is used for image processing in Rust.
- x86_64 warnings on Apple Silicon are expected — Rust builds arm64 by default.
- Legacy MethodChannel files are kept for reference but not actively used.
