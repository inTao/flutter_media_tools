import 'package:flutter/material.dart';
import 'package:flutter_media_tools/flutter_media_tools.dart';

import 'media_formatters.dart';

class InfoRow {
  const InfoRow(this.label, this.value);

  final String label;
  final String value;
}

List<InfoRow> rowsFor(BaseMediaInfo info) {
  final List<InfoRow> rows = <InfoRow>[
    InfoRow('Format', formatNameFor(info)),
    InfoRow('Media type', info.mediaType ?? 'unknown'),
    InfoRow('File size', formatBytes(info.fileSize)),
  ];

  switch (info) {
    case ImageMediaInfo():
      rows.addAll(<InfoRow>[
        InfoRow('Dimensions', '${info.width} x ${info.height}'),
        InfoRow('Codec', info.codec ?? 'unknown'),
        InfoRow('Bitrate', formatBitrate(info.bitrate)),
      ]);
    case VideoMediaInfo():
      rows.addAll(<InfoRow>[
        InfoRow('Dimensions', '${info.width} x ${info.height}'),
        InfoRow('Duration', formatDuration(info.duration)),
        InfoRow('Frame rate', formatFrameRate(info.frameRate)),
        InfoRow('Codec', info.codec ?? 'unknown'),
        InfoRow('Bitrate', formatBitrate(info.bitrate)),
      ]);
    case AudioMediaInfo():
      rows.addAll(<InfoRow>[
        InfoRow('Duration', formatDuration(info.duration)),
        InfoRow('Sample rate', formatSampleRate(info.sampleRate)),
        InfoRow('Channels', info.channels?.toString() ?? 'unknown'),
        InfoRow('Codec', info.codec ?? 'unknown'),
        InfoRow('Bitrate', formatBitrate(info.bitrate)),
      ]);
  }

  return rows;
}

String formatNameFor(BaseMediaInfo info) => switch (info) {
  ImageMediaInfo() => info.format.name,
  VideoMediaInfo() => info.format.name,
  AudioMediaInfo() => info.format.name,
};

IconData iconFor(BaseMediaInfo info) => switch (info) {
  ImageMediaInfo() => Icons.image_outlined,
  VideoMediaInfo() => Icons.movie_outlined,
  AudioMediaInfo() => Icons.graphic_eq,
};

String titleFor(BaseMediaInfo info) => switch (info) {
  ImageMediaInfo() => 'ImageMediaInfo',
  VideoMediaInfo() => 'VideoMediaInfo',
  AudioMediaInfo() => 'AudioMediaInfo',
};
