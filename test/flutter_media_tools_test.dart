import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_media_tools/flutter_media_tools.dart';

void main() {
  test('MediaToolsNative greet returns greeting', () {
    final result = MediaToolsNative.instance.greet('Test');
    expect(result, contains('Test'));
    expect(result, contains('Rust'));
  });
}
