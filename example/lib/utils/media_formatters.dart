String formatBytes(int bytes) {
  const List<String> units = <String>['B', 'KB', 'MB', 'GB'];
  double value = bytes.toDouble();
  int unitIndex = 0;
  while (value >= 1024 && unitIndex < units.length - 1) {
    value /= 1024;
    unitIndex += 1;
  }
  final int fractionDigits = value >= 10 || unitIndex == 0 ? 0 : 1;
  return '${value.toStringAsFixed(fractionDigits)} ${units[unitIndex]}';
}

String formatDuration(Duration duration) {
  final int minutes = duration.inMinutes;
  final int seconds = duration.inSeconds.remainder(60);
  final int milliseconds = duration.inMilliseconds.remainder(1000);
  return '$minutes:${seconds.toString().padLeft(2, '0')}.'
      '${milliseconds.toString().padLeft(3, '0')}';
}

String formatBitrate(int? bitrate) {
  if (bitrate == null) {
    return 'unknown';
  }
  return '${(bitrate / 1000).toStringAsFixed(1)} kbps';
}

String formatFrameRate(double? frameRate) {
  if (frameRate == null) {
    return 'unknown';
  }
  return '${frameRate.toStringAsFixed(2)} fps';
}

String formatSampleRate(int? sampleRate) {
  if (sampleRate == null) {
    return 'unknown';
  }
  return '${(sampleRate / 1000).toStringAsFixed(1)} kHz';
}
