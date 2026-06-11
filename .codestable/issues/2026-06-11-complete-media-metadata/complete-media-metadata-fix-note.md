---
doc_type: issue-fix
issue: 2026-06-11-complete-media-metadata
path: fast-track
fix_date: 2026-06-11
tags: [media, metadata, native-probe]
---

# Complete Media Metadata 修复记录

## 1. 问题描述

`getMediaInfo` 返回的 `metadata` 过少，当前 native probe 基本只输出 `container`，图片 EXIF 也只输出 `present` 和 `orientation`。这和媒体信息能力设计中“读取容器 metadata 和图片 EXIF”的预期不一致。

## 2. 根因

`src/media_probe.c` 的 JSON 生成阶段只硬编码了 `metadata.container`，各格式 parser 只把宽高、时长、codec、bitrate 等结构化字段填到 subtype 对象，没有把同一次扫描中读到的容器字段、资源字段和原始 tag 收集到 metadata map。

## 3. 修复方案

新增 native `MetadataMap` 收集器，让格式 parser 在读取媒体资源数据时同步记录 metadata：

- 图片：PNG/GIF/BMP/WebP/JPEG 写入尺寸、codec、bit depth、density、WebP feature flags 等可读字段。
- EXIF：JPEG APP1 改为遍历 TIFF IFD，读取常见 tag，并用 `tag.0x....` 保留未知 tag。
- WAV：读取 `fmt ` 资源字段和 `LIST/INFO` 文本标签。
- MP4/MOV：统一 box 读取，采集 `ftyp`、`mvhd`、track 资源字段，以及常见 `udta/meta/ilst` tag。

顶层结构化字段仍然保留，`metadata` 作为可展示/追溯的字符串 map 输出。

## 4. 改动文件清单

- `src/media_probe.c` — 扩展各格式 parser，统一从解析结果生成结构化字段与 metadata/exif JSON。
- `src/media_metadata.h` — 新增 native metadata map 类型和操作声明。
- `src/media_metadata.c` — 新增 metadata map 的插入、去重、复制、字节清洗和释放实现。
- `hook/build.dart` — native build hook 编译新增 C 源文件。
- `test/flutter_media_tools_test.dart` — 补充 metadata / EXIF / WAV INFO / MP4 资源字段断言。

## 5. 验证结果

- `dart format hook/build.dart test/flutter_media_tools_test.dart lib/src/media_info.dart lib/src/media_info_native.dart lib/flutter_media_tools.dart`：通过。
- `dart analyze`：通过，No issues found。
- `dart test`：通过，14 个测试全部通过。

## 6. 遗留事项

当前实现仍是内置轻量 parser，不依赖 FFmpeg/MediaInfo；已经覆盖现有支持格式中可低风险解析的常见 metadata。更深的私有厂商 tag、完整 ICC/XMP、复杂 MP4 cover art 内容解码等可后续按格式单独扩展。
