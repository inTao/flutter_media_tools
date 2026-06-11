---
doc_type: feature-ff-note
feature: typed-media-info
date: 2026-06-11
requirement: media-file-info
tags: [media, public-api, dart]
---

## 做了什么
让 `getMediaInfo` 支持泛型 subtype 返回，调用方已知媒体类型时可以直接得到 `ImageMediaInfo` / `VideoMediaInfo` / `AudioMediaInfo`，不再需要手写 `as`。原来的 `BaseMediaInfo` 调用方式保持兼容。

## 改了哪些
- `lib/src/media_info_native.dart` — 将 `getMediaInfo` 改为泛型入口，并在 probe 后校验返回 subtype。
- `lib/flutter_media_tools.dart` — 公开转发泛型签名，并补充 typed 调用示例。
- `test/flutter_media_tools_test.dart` — 覆盖赋值上下文推断、显式类型参数和 subtype 不匹配异常。
- `README.md` — 补充外部调用方的 typed 用法示例。

## 怎么验证的
已运行 `dart format lib/src/media_info_native.dart lib/flutter_media_tools.dart test/flutter_media_tools_test.dart`、`dart analyze`、`dart test`。分析无问题，测试 13 个全部通过。
