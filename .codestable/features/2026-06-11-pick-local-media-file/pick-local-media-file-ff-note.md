---
doc_type: feature-ff-note
feature: pick-local-media-file
date: 2026-06-11
requirement:
tags: [example, local-file, media]
---

## 做了什么
把 example 从手动输入路径改为选择本地媒体文件后自动调用 `getMediaInfo`，让 API 示例更贴近日常使用方式。

## 改了哪些
- `example/lib/services/media_file_picker.dart` — 新增本地媒体文件选择服务，封装 `file_selector.openFile`。
- `example/lib/pages/media_info_page.dart` — 页面状态改为保存已选文件并用选中文件路径调用 `getMediaInfo`。
- `example/lib/widgets/probe_panel.dart` — 输入框改为选择文件 / 重新分析 / 清空的操作面板。
- `example/pubspec.yaml` / 平台生成文件 — 引入 `file_selector` 并注册桌面平台插件。
- `example/macos/Runner/*.entitlements` — 为 macOS 沙盒应用增加用户选择文件的只读权限。

## 怎么验证的
已运行 `dart format example/lib`、`flutter analyze example`、`dart test`。example 目录没有 `test/`，所以未单独跑 `flutter test`。
