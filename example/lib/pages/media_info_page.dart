import 'package:flutter/material.dart';
import 'package:flutter_media_tools/flutter_media_tools.dart';

import '../widgets/empty_panel.dart';
import '../widgets/error_panel.dart';
import '../widgets/media_info_result.dart';
import '../widgets/probe_panel.dart';

class MediaInfoPage extends StatefulWidget {
  const MediaInfoPage({super.key});

  @override
  State<MediaInfoPage> createState() => _MediaInfoPageState();
}

class _MediaInfoPageState extends State<MediaInfoPage> {
  final TextEditingController _pathController = TextEditingController();

  BaseMediaInfo? _info;
  String? _error;
  String? _lastPath;
  bool _isLoading = false;

  @override
  void dispose() {
    _pathController.dispose();
    super.dispose();
  }

  Future<void> _readMediaInfo() async {
    final String path = _pathController.text.trim();
    setState(() {
      _isLoading = true;
      _error = null;
      _info = null;
      _lastPath = path;
    });

    try {
      // The public API returns the shared base type, then callers can branch on
      // the concrete subtype for media-specific fields.
      final BaseMediaInfo info = await getMediaInfo(path);
      if (!mounted) {
        return;
      }
      setState(() {
        _info = info;
      });
    } on MediaInfoException catch (error) {
      if (!mounted) {
        return;
      }
      setState(() {
        _error = error.message;
      });
    } catch (error) {
      if (!mounted) {
        return;
      }
      setState(() {
        _error = error.toString();
      });
    } finally {
      if (mounted) {
        setState(() {
          _isLoading = false;
        });
      }
    }
  }

  void _clearPath() {
    _pathController.clear();
    setState(() {
      _error = null;
      _info = null;
      _lastPath = null;
    });
  }

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(title: const Text('Flutter Media Tools')),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.all(24),
          children: <Widget>[
            Center(
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 840),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: <Widget>[
                    Text('Media info', style: theme.textTheme.headlineMedium),
                    const SizedBox(height: 20),
                    ProbePanel(
                      controller: _pathController,
                      isLoading: _isLoading,
                      onClear: _clearPath,
                      onSubmit: _readMediaInfo,
                    ),
                    const SizedBox(height: 20),
                    if (_isLoading) const LinearProgressIndicator(),
                    if (_error != null) ...<Widget>[
                      if (_isLoading) const SizedBox(height: 16),
                      ErrorPanel(path: _lastPath, message: _error!),
                    ] else if (_info != null) ...<Widget>[
                      if (_isLoading) const SizedBox(height: 16),
                      MediaInfoResult(info: _info!),
                    ] else if (!_isLoading) ...<Widget>[
                      EmptyPanel(textStyle: theme.textTheme.bodyLarge),
                    ],
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
