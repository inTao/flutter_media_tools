import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';

import 'package:flutter_media_tools/flutter_media_tools.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('greet test', (WidgetTester tester) async {
    final result = MediaToolsNative.instance.greet('Integration');
    expect(result, contains('Integration'));
  });
}
