import 'dart:io';
import 'dart:typed_data';

import 'package:test/test.dart';

import 'package:flutter_media_tools/flutter_media_tools.dart';

void main() {
  group('getMediaInfo', () {
    test('reads PNG image information', () async {
      final BaseMediaInfo info = await getMediaInfo(_fixturePath('sample.png'));

      expect(info, isA<ImageMediaInfo>());
      expect(info.fileSize, greaterThan(0));
      expect(info.metadata['container'], 'PNG');
      expect(info.format, ImageFormat.png);
      expect(info.mediaType, 'image/png');

      final ImageMediaInfo image = info as ImageMediaInfo;
      expect(image.format, ImageFormat.png);
      expect(image.format.mediaType, 'image/png');
      expect(image.width, 3);
      expect(image.height, 2);
      expect(image.codec, 'png');
      expect(image.bitrate, isNull);
      expect(image.exif, isEmpty);
    });

    test('infers the requested subtype from assignment context', () async {
      final ImageMediaInfo image = await getMediaInfo(
        _fixturePath('sample.png'),
      );

      expect(image.format, ImageFormat.png);
      expect(image.width, 3);
      expect(image.height, 2);
    });

    test(
      'returns the requested subtype with an explicit type argument',
      () async {
        final AudioMediaInfo audio = await getMediaInfo<AudioMediaInfo>(
          _fixturePath('sample.wav'),
        );

        expect(audio.format, AudioFormat.wave);
        expect(audio.duration.inMilliseconds, closeTo(250, 5));
      },
    );

    test('reads WebP image information with enum format', () async {
      final BaseMediaInfo info = await getMediaInfo(
        _fixturePath('sample.webp'),
      );

      expect(info, isA<ImageMediaInfo>());
      final ImageMediaInfo image = info as ImageMediaInfo;
      expect(image.format, ImageFormat.webp);
      expect(image.format.mediaType, 'image/webp');
      expect(image.width, 4);
      expect(image.height, 3);
    });

    test('reads JPEG EXIF information when present', () async {
      final Directory directory = await Directory.systemTemp.createTemp(
        'flutter_media_tools_test_',
      );
      addTearDown(() => directory.delete(recursive: true));
      final File image = File('${directory.path}/oriented.jpg');
      await image.writeAsBytes(_jpegWithExifOrientation(6));

      final BaseMediaInfo info = await getMediaInfo(image.path);

      expect(info, isA<ImageMediaInfo>());
      final ImageMediaInfo jpeg = info as ImageMediaInfo;
      expect(jpeg.format, ImageFormat.jpeg);
      expect(jpeg.format.mediaType, 'image/jpeg');
      expect(jpeg.width, 5);
      expect(jpeg.height, 4);
      expect(jpeg.exif['present'], 'true');
      expect(jpeg.exif['orientation'], '6');
    });

    test('returns an empty EXIF map when no EXIF exists', () async {
      final BaseMediaInfo info = await getMediaInfo(_fixturePath('sample.png'));

      expect(info, isA<ImageMediaInfo>());
      expect((info as ImageMediaInfo).exif, isEmpty);
    });

    test('reads WAV audio information', () async {
      final BaseMediaInfo info = await getMediaInfo(_fixturePath('sample.wav'));

      expect(info, isA<AudioMediaInfo>());
      final AudioMediaInfo audio = info as AudioMediaInfo;
      expect(audio.format, AudioFormat.wave);
      expect(audio.format.mediaType, 'audio/vnd.wave');
      expect(audio.duration.inMilliseconds, closeTo(250, 5));
      expect(audio.sampleRate, 8000);
      expect(audio.channels, 1);
      expect(audio.codec, 'pcm');
      expect(audio.bitrate, greaterThan(0));
      expect(audio.metadata['container'], 'WAVE');
    });

    test('reads MP4 video information', () async {
      final BaseMediaInfo info = await getMediaInfo(_fixturePath('sample.mp4'));

      expect(info, isA<VideoMediaInfo>());
      final VideoMediaInfo video = info as VideoMediaInfo;
      expect(video.format, VideoFormat.mp4);
      expect(video.format.mediaType, 'video/mp4');
      expect(video.width, 6);
      expect(video.height, 4);
      expect(video.duration.inMilliseconds, closeTo(500, 5));
      expect(video.codec, 'avc1');
      expect(video.bitrate, greaterThan(0));
      expect(video.frameRate, closeTo(2, 0.01));
      expect(video.metadata['container'], 'MP4');
    });

    test('detects format from content instead of file extension', () async {
      final Directory directory = await Directory.systemTemp.createTemp(
        'flutter_media_tools_test_',
      );
      addTearDown(() => directory.delete(recursive: true));
      final File copied = File('${directory.path}/renamed.txt');
      await File(_fixturePath('sample.png')).copy(copied.path);

      final BaseMediaInfo info = await getMediaInfo(copied.path);

      expect(info, isA<ImageMediaInfo>());
      expect((info as ImageMediaInfo).format, ImageFormat.png);
    });

    test('throws typed exception for missing files', () {
      expect(
        () => getMediaInfo(_fixturePath('missing.png')),
        throwsA(isA<MediaFileNotFoundException>()),
      );
    });

    test('throws typed exception for unsupported files', () async {
      final Directory directory = await Directory.systemTemp.createTemp(
        'flutter_media_tools_test_',
      );
      addTearDown(() => directory.delete(recursive: true));
      final File text = File('${directory.path}/note.txt');
      await text.writeAsString('not a media file');

      expect(
        () => getMediaInfo(text.path),
        throwsA(isA<UnsupportedMediaFileException>()),
      );
    });

    test('throws typed exception when the requested subtype differs', () {
      expect(
        () => getMediaInfo<VideoMediaInfo>(_fixturePath('sample.png')),
        throwsA(
          isA<UnsupportedMediaFileException>().having(
            (UnsupportedMediaFileException error) => error.message,
            'message',
            contains('Expected media info of type VideoMediaInfo'),
          ),
        ),
      );
    });

    test('rejects remote URLs', () {
      expect(
        () => getMediaInfo('https://example.com/sample.png'),
        throwsA(isA<UnsupportedMediaFileException>()),
      );
    });
  });
}

String _fixturePath(String name) => 'test/fixtures/$name';

Uint8List _jpegWithExifOrientation(int orientation) {
  final int low = orientation & 0xff;
  final int high = (orientation >> 8) & 0xff;
  return Uint8List.fromList(<int>[
    0xff, 0xd8, // SOI
    0xff, 0xe1, 0x00, 0x22, // APP1, 32-byte payload
    0x45, 0x78, 0x69, 0x66, 0x00, 0x00, // Exif\0\0
    0x49, 0x49, 0x2a, 0x00, // little-endian TIFF
    0x08, 0x00, 0x00, 0x00, // first IFD offset
    0x01, 0x00, // entry count
    0x12, 0x01, // orientation tag
    0x03, 0x00, // SHORT
    0x01, 0x00, 0x00, 0x00, // count
    low, high, 0x00, 0x00, // value
    0x00, 0x00, 0x00, 0x00, // next IFD
    0xff, 0xc0, 0x00, 0x0b, // SOF0
    0x08, 0x00, 0x04, 0x00, 0x05, // precision, height, width
    0x01, 0x01, 0x11, 0x00, // one component
    0xff, 0xd9, // EOI
  ]);
}
