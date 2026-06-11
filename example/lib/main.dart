import 'package:flutter/material.dart';
import 'package:flutter_media_tools/flutter_media_tools.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  String _greeting = 'Loading...';

  @override
  void initState() {
    super.initState();
    setState(() {
      _greeting = MediaToolsNative.instance.greet('Flutter');
    });
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(title: const Text('Media Tools Example')),
        body: Center(child: Text(_greeting)),
      ),
    );
  }
}
