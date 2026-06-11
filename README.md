# flutter_media_tools

Flutter media tools FFI package.

## Usage

Use `getMediaInfo` to read structured information from a local media file.

```dart
import 'package:flutter_media_tools/flutter_media_tools.dart';

final info = await getMediaInfo('/path/to/file.mp4');
```

Ask for a concrete subtype when the caller already expects a media kind.

```dart
final ImageMediaInfo image = await getMediaInfo('/path/to/photo.png');
final video = await getMediaInfo<VideoMediaInfo>('/path/to/file.mp4');
```

## Project structure

* `src`: Contains the native source code.

* `lib`: Contains the Dart code that defines the API of the plugin, and which
  calls into the native code using `dart:ffi`.

* `hook`: Contains `build.dart`, the build hook that compiles and bundles the
  native code.

## Building and bundling native code

`hook/build.dart` does the building of native components.

Bundling is done by Flutter based on the output from `build.dart`.

## Binding to native code

To use the native code, bindings in Dart are needed.
To avoid writing these by hand, they are generated from the header file
(`src/media_probe.h`) by `package:ffigen`.
Regenerate the bindings by running `dart run ffigen --config ffigen.yaml`.
