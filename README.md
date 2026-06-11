# flutter_media_tools

A Flutter plugin with a Rust-powered core for media processing.

## Architecture

```
Flutter (Dart) → dart:ffi → Rust (cdylib)
```

Image processing (resize, rotate, crop, blur, grayscale, format conversion)
runs in Rust for performance. Dart communicates via C-compatible FFI functions.

## Getting Started

### Prerequisites

- Flutter SDK
- Rust toolchain (`rustup`, `cargo`)

### Build

```bash
# 1. Compile Rust library for your platform
./native/build.sh macos   # or ios, linux, windows, android, all

# 2. Run the example app
cd example && flutter run -d macos
```

### Use in your project

```dart
import 'package:flutter_media_tools/flutter_media_tools.dart';

final native = MediaToolsNative.instance;

// Get image info
final info = native.imageInfo(imageBytes);
print('${info.width}x${info.height} ${info.format}');

// Resize
final resized = native.resizeImage(imageBytes, 200, 200);

// Rotate, crop, blur, grayscale, convert format...
final gray = native.grayscaleImage(imageBytes);
final png = native.convertFormat(imageBytes, 'png');
```

## Supported Operations

| Function | Description |
|----------|-------------|
| `greet(name)` | Test connectivity to Rust |
| `imageInfo(data)` | Get width, height, format |
| `resizeImage(data, w, h)` | Resize to exact dimensions |
| `rotateImage(data, degrees)` | Rotate by 90/180/270 |
| `cropImage(data, x, y, w, h)` | Crop region |
| `blurImage(data, sigma)` | Gaussian blur |
| `grayscaleImage(data)` | Convert to grayscale |
| `convertFormat(data, format)` | Convert between png/jpeg/webp/bmp |

## Development

See [AGENTS.md](AGENTS.md) for architecture details and development workflow.
