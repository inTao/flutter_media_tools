import 'package:file_selector/file_selector.dart';

class MediaFileSelection {
  const MediaFileSelection({required this.name, required this.path});

  final String name;
  final String path;
}

class MediaFilePicker {
  const MediaFilePicker();

  Future<MediaFileSelection?> pickMediaFile() async {
    final XFile? file = await openFile(
      acceptedTypeGroups: const <XTypeGroup>[
        XTypeGroup(
          label: 'Media files',
          extensions: <String>[
            'aac',
            'avi',
            'bmp',
            'flac',
            'gif',
            'heic',
            'jpeg',
            'jpg',
            'm4a',
            'mkv',
            'mov',
            'mp3',
            'mp4',
            'ogg',
            'png',
            'tif',
            'tiff',
            'wav',
            'webm',
            'webp',
          ],
          mimeTypes: <String>['audio/*', 'image/*', 'video/*'],
        ),
      ],
    );

    final String? path = file?.path;
    if (file == null || path == null || path.isEmpty) {
      return null;
    }
    return MediaFileSelection(name: file.name, path: path);
  }
}
