/// Shared base type for all supported local media files.
sealed class BaseMediaInfo {
  const BaseMediaInfo({
    required this.path,
    required this.fileSize,
    required this.metadata,
  });

  /// Original local path passed to the probe.
  final String path;

  /// Actual file size in bytes.
  final int fileSize;

  /// Detected media format.
  MediaFormat get format;

  /// Registered media type string for [format] when known.
  String? get mediaType => format.mediaType;

  /// Container-level metadata read from the file.
  final Map<String, String> metadata;
}

/// Common interface implemented by supported media format enums.
abstract interface class MediaFormat {
  /// Registered media type string for this format when known.
  String? get mediaType;
}

/// Supported image formats.
enum ImageFormat implements MediaFormat {
  jpeg('image/jpeg'),
  png('image/png'),
  webp('image/webp'),
  gif('image/gif'),
  heic('image/heic'),
  tiff('image/tiff'),
  bmp('image/bmp'),
  unknown(null);

  const ImageFormat(this.mediaType);

  /// Registered media type string for this image format when known.
  @override
  final String? mediaType;
}

/// Supported video formats.
enum VideoFormat implements MediaFormat {
  mp4('video/mp4'),
  quicktime('video/quicktime'),
  matroska('video/matroska'),
  webm('video/webm'),
  avi('video/vnd.avi'),
  unknown(null);

  const VideoFormat(this.mediaType);

  /// Registered media type string for this video format when known.
  @override
  final String? mediaType;
}

/// Supported audio formats.
enum AudioFormat implements MediaFormat {
  mpeg('audio/mpeg'),
  aac('audio/aac'),
  wave('audio/vnd.wave'),
  flac('audio/flac'),
  mp4('audio/mp4'),
  ogg('audio/ogg'),
  unknown(null);

  const AudioFormat(this.mediaType);

  /// Registered media type string for this audio format when known.
  @override
  final String? mediaType;
}

/// Structured information for an image file.
final class ImageMediaInfo extends BaseMediaInfo {
  const ImageMediaInfo({
    required super.path,
    required super.fileSize,
    required super.metadata,
    required this.format,
    required this.width,
    required this.height,
    required this.exif,
    this.codec,
    this.bitrate,
  });

  @override
  final ImageFormat format;
  final int width;
  final int height;
  final String? codec;
  final int? bitrate;
  final Map<String, String> exif;
}

/// Structured information for a video file.
final class VideoMediaInfo extends BaseMediaInfo {
  const VideoMediaInfo({
    required super.path,
    required super.fileSize,
    required super.metadata,
    required this.format,
    required this.width,
    required this.height,
    required this.duration,
    this.frameRate,
    this.codec,
    this.bitrate,
  });

  @override
  final VideoFormat format;
  final int width;
  final int height;
  final Duration duration;
  final double? frameRate;
  final String? codec;
  final int? bitrate;
}

/// Structured information for an audio file.
final class AudioMediaInfo extends BaseMediaInfo {
  const AudioMediaInfo({
    required super.path,
    required super.fileSize,
    required super.metadata,
    required this.format,
    required this.duration,
    this.sampleRate,
    this.channels,
    this.codec,
    this.bitrate,
  });

  @override
  final AudioFormat format;
  final Duration duration;
  final int? sampleRate;
  final int? channels;
  final String? codec;
  final int? bitrate;
}

/// Base exception for media information probing failures.
sealed class MediaInfoException implements Exception {
  const MediaInfoException(this.path, this.message);

  final String path;
  final String message;

  @override
  String toString() => '$runtimeType: $message ($path)';
}

/// The requested file path does not exist or cannot be read.
final class MediaFileNotFoundException extends MediaInfoException {
  const MediaFileNotFoundException(super.path, super.message);
}

/// The file exists but is not a supported image, video, or audio file.
final class UnsupportedMediaFileException extends MediaInfoException {
  const UnsupportedMediaFileException(super.path, super.message);
}

/// Native probing failed after accepting the file path.
final class MediaProbeException extends MediaInfoException {
  const MediaProbeException(super.path, super.message);
}
