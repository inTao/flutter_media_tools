import 'package:flutter/material.dart';

import '../services/media_file_picker.dart';

class ProbePanel extends StatelessWidget {
  const ProbePanel({
    required this.isLoading,
    required this.selection,
    required this.onClear,
    required this.onPick,
    required this.onAnalyzeAgain,
    super.key,
  });

  final bool isLoading;
  final MediaFileSelection? selection;
  final VoidCallback onClear;
  final VoidCallback onPick;
  final VoidCallback onAnalyzeAgain;

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    final MediaFileSelection? selectedFile = selection;
    final Widget selectedFileView = _SelectedFileView(selection: selectedFile);
    final Widget pickButton = FilledButton.icon(
      icon: const Icon(Icons.folder_open),
      label: Text(selectedFile == null ? 'Pick media file' : 'Choose another'),
      onPressed: isLoading ? null : onPick,
    );
    final Widget clearButton = IconButton.filledTonal(
      icon: const Icon(Icons.clear),
      tooltip: 'Clear',
      onPressed: isLoading || selectedFile == null ? null : onClear,
    );
    final Widget analyzeAgainButton = OutlinedButton.icon(
      icon: const Icon(Icons.search),
      label: const Text('Analyze again'),
      onPressed: isLoading || selectedFile == null ? null : onAnalyzeAgain,
    );

    return DecoratedBox(
      decoration: BoxDecoration(
        border: Border.all(color: theme.colorScheme.outlineVariant),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: LayoutBuilder(
          builder: (BuildContext context, BoxConstraints constraints) {
            if (constraints.maxWidth < 560) {
              return Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: <Widget>[
                  selectedFileView,
                  const SizedBox(height: 12),
                  pickButton,
                  const SizedBox(height: 8),
                  Row(
                    children: <Widget>[
                      clearButton,
                      const SizedBox(width: 8),
                      Expanded(child: analyzeAgainButton),
                    ],
                  ),
                ],
              );
            }

            return Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Expanded(child: selectedFileView),
                const SizedBox(width: 12),
                pickButton,
                const SizedBox(width: 8),
                clearButton,
                const SizedBox(width: 8),
                analyzeAgainButton,
              ],
            );
          },
        ),
      ),
    );
  }
}

class _SelectedFileView extends StatelessWidget {
  const _SelectedFileView({required this.selection});

  final MediaFileSelection? selection;

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    final MediaFileSelection? selectedFile = selection;

    return DecoratedBox(
      decoration: BoxDecoration(
        color: theme.colorScheme.surface,
        border: Border.all(color: theme.colorScheme.outlineVariant),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
        child: Row(
          children: <Widget>[
            Icon(
              selectedFile == null
                  ? Icons.perm_media_outlined
                  : Icons.insert_drive_file_outlined,
              color: theme.colorScheme.primary,
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                mainAxisSize: MainAxisSize.min,
                children: <Widget>[
                  Text(
                    selectedFile?.name ?? 'No local file selected',
                    style: theme.textTheme.bodyLarge,
                    overflow: TextOverflow.ellipsis,
                  ),
                  if (selectedFile != null) ...<Widget>[
                    const SizedBox(height: 2),
                    Text(
                      selectedFile.path,
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: theme.colorScheme.onSurfaceVariant,
                      ),
                      overflow: TextOverflow.ellipsis,
                    ),
                  ],
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
