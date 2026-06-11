import 'src/media_info.dart';
import 'src/media_info_native.dart' as media_info_native;

export 'src/media_info.dart';

/// Reads structured information from a single local media file.
///
/// Provide a concrete subtype to get a checked typed result:
///
/// ```dart
/// final ImageMediaInfo image = await getMediaInfo('/tmp/photo.png');
/// final video = await getMediaInfo<VideoMediaInfo>('/tmp/video.mp4');
/// ```
Future<T> getMediaInfo<T extends BaseMediaInfo>(String path) =>
    media_info_native.getMediaInfo<T>(path);
