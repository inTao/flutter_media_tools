import 'package:flutter/material.dart';
import 'package:flutter_media_tools/flutter_media_tools.dart';

import '../services/media_file_picker.dart';
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
  final MediaFilePicker _filePicker = const MediaFilePicker();

  BaseMediaInfo? _info;
  String? _error;
  MediaFileSelection? _selection;
  String? _lastPath;
  bool _isLoading = false;

  Future<void> _pickAndReadMediaInfo() async {
    final MediaFileSelection? selection = await _filePicker.pickMediaFile();
    if (selection == null) {
      return;
    }
    await _readMediaInfo(selection);
  }

  Future<void> _readSelectedMediaInfo() async {
    final MediaFileSelection? selection = _selection;
    if (selection == null) {
      return;
    }
    await _readMediaInfo(selection);
  }

  Future<void> _readMediaInfo(MediaFileSelection selection) async {
    setState(() {
      _isLoading = true;
      _error = null;
      _info = null;
      _selection = selection;
      _lastPath = selection.path;
    });

    try {
      // The public API returns the shared base type, then callers can branch on
      // the concrete subtype for media-specific fields.
      final BaseMediaInfo info = await getMediaInfo(selection.path);
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

  void _clearSelection() {
    setState(() {
      _error = null;
      _info = null;
      _selection = null;
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
                      isLoading: _isLoading,
                      selection: _selection,
                      onClear: _clearSelection,
                      onPick: _pickAndReadMediaInfo,
                      onAnalyzeAgain: _readSelectedMediaInfo,
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
