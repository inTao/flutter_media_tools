import 'package:flutter/material.dart';

import 'pages/media_info_page.dart';

class MediaToolsExampleApp extends StatelessWidget {
  const MediaToolsExampleApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'Flutter Media Tools',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: const Color(0xff0a6f68)),
        useMaterial3: true,
      ),
      home: const MediaInfoPage(),
    );
  }
}
