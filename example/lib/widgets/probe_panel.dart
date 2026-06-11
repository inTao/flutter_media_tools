import 'package:flutter/material.dart';

class ProbePanel extends StatelessWidget {
  const ProbePanel({
    required this.controller,
    required this.isLoading,
    required this.onClear,
    required this.onSubmit,
    super.key,
  });

  final TextEditingController controller;
  final bool isLoading;
  final VoidCallback onClear;
  final VoidCallback onSubmit;

  @override
  Widget build(BuildContext context) {
    final Widget pathField = TextField(
      controller: controller,
      enabled: !isLoading,
      decoration: const InputDecoration(
        border: OutlineInputBorder(),
        labelText: 'Local media path',
        prefixIcon: Icon(Icons.folder_open),
      ),
      onSubmitted: (_) {
        if (!isLoading) {
          onSubmit();
        }
      },
    );
    final Widget clearButton = IconButton.filledTonal(
      icon: const Icon(Icons.clear),
      tooltip: 'Clear',
      onPressed: isLoading ? null : onClear,
    );
    final Widget analyzeButton = FilledButton.icon(
      icon: const Icon(Icons.search),
      label: const Text('Analyze'),
      onPressed: isLoading ? null : onSubmit,
    );

    return DecoratedBox(
      decoration: BoxDecoration(
        border: Border.all(color: Theme.of(context).colorScheme.outlineVariant),
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
                  pathField,
                  const SizedBox(height: 12),
                  Row(
                    children: <Widget>[
                      clearButton,
                      const SizedBox(width: 8),
                      Expanded(child: analyzeButton),
                    ],
                  ),
                ],
              );
            }

            return Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Expanded(child: pathField),
                const SizedBox(width: 12),
                clearButton,
                const SizedBox(width: 8),
                analyzeButton,
              ],
            );
          },
        ),
      ),
    );
  }
}
