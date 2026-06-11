import 'package:flutter/material.dart';
import 'package:flutter_media_tools/flutter_media_tools.dart';

import '../utils/media_info_display.dart';
import 'info_chip.dart';
import 'key_value_line.dart';

class MediaInfoResult extends StatelessWidget {
  const MediaInfoResult({required this.info, super.key});

  final BaseMediaInfo info;

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    final List<InfoRow> rows = rowsFor(info);
    final BaseMediaInfo mediaInfo = info;
    final Map<String, String> exif = mediaInfo is ImageMediaInfo
        ? mediaInfo.exif
        : const <String, String>{};

    return DecoratedBox(
      decoration: BoxDecoration(
        color: theme.colorScheme.surfaceContainerHighest,
        borderRadius: BorderRadius.circular(8),
      ),
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Row(
              children: <Widget>[
                Icon(iconFor(info), color: theme.colorScheme.primary),
                const SizedBox(width: 8),
                Flexible(
                  child: Text(
                    titleFor(info),
                    style: theme.textTheme.titleLarge,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 16),
            Wrap(
              runSpacing: 12,
              spacing: 12,
              children: rows.map(InfoChip.new).toList(),
            ),
            if (info.metadata.isNotEmpty) ...<Widget>[
              const SizedBox(height: 20),
              Text('Metadata', style: theme.textTheme.titleMedium),
              const SizedBox(height: 8),
              ...info.metadata.entries.map(
                (MapEntry<String, String> entry) =>
                    KeyValueLine(label: entry.key, value: entry.value),
              ),
            ],
            if (exif.isNotEmpty) ...<Widget>[
              const SizedBox(height: 20),
              Text('EXIF', style: theme.textTheme.titleMedium),
              const SizedBox(height: 8),
              ...exif.entries.map(
                (MapEntry<String, String> entry) =>
                    KeyValueLine(label: entry.key, value: entry.value),
              ),
            ],
          ],
        ),
      ),
    );
  }
}
