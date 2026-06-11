#include "media_probe.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} JsonBuilder;

typedef struct {
  int width;
  int height;
  const char *format;
  const char *codec;
  const char *container;
  bool has_exif;
  int exif_orientation;
} ImageInfo;

typedef struct {
  int width;
  int height;
  int64_t duration_micros;
  double frame_rate;
  int bitrate;
  char codec[8];
  const char *format;
  const char *container;
} VideoInfo;

typedef struct {
  int64_t duration_micros;
  int sample_rate;
  int channels;
  int bitrate;
  char codec[8];
  const char *format;
  const char *container;
} AudioInfo;

typedef struct {
  int width;
  int height;
  uint32_t duration_timescale;
  uint64_t duration_units;
  uint64_t sample_count;
  char handler[5];
  char codec[8];
  int sample_rate;
  int channels;
} Mp4Track;

static bool jb_init(JsonBuilder *builder) {
  builder->capacity = 2048;
  builder->length = 0;
  builder->data = (char *)malloc(builder->capacity);
  if (builder->data == NULL) {
    return false;
  }
  builder->data[0] = '\0';
  return true;
}

static bool jb_reserve(JsonBuilder *builder, size_t extra) {
  if (builder->length + extra + 1 <= builder->capacity) {
    return true;
  }
  size_t next_capacity = builder->capacity;
  while (builder->length + extra + 1 > next_capacity) {
    next_capacity *= 2;
  }
  char *next = (char *)realloc(builder->data, next_capacity);
  if (next == NULL) {
    return false;
  }
  builder->data = next;
  builder->capacity = next_capacity;
  return true;
}

static bool jb_append(JsonBuilder *builder, const char *text) {
  size_t length = strlen(text);
  if (!jb_reserve(builder, length)) {
    return false;
  }
  memcpy(builder->data + builder->length, text, length + 1);
  builder->length += length;
  return true;
}

static bool jb_appendf(JsonBuilder *builder, const char *format, ...) {
  va_list args;
  va_start(args, format);
  va_list args_copy;
  va_copy(args_copy, args);
  int needed = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);
  if (needed < 0) {
    va_end(args);
    return false;
  }
  if (!jb_reserve(builder, (size_t)needed)) {
    va_end(args);
    return false;
  }
  vsnprintf(builder->data + builder->length, (size_t)needed + 1, format, args);
  builder->length += (size_t)needed;
  va_end(args);
  return true;
}

static bool jb_append_json_string(JsonBuilder *builder, const char *text) {
  if (!jb_append(builder, "\"")) {
    return false;
  }
  for (const unsigned char *cursor = (const unsigned char *)text; *cursor;
       cursor++) {
    switch (*cursor) {
      case '"':
        if (!jb_append(builder, "\\\"")) return false;
        break;
      case '\\':
        if (!jb_append(builder, "\\\\")) return false;
        break;
      case '\b':
        if (!jb_append(builder, "\\b")) return false;
        break;
      case '\f':
        if (!jb_append(builder, "\\f")) return false;
        break;
      case '\n':
        if (!jb_append(builder, "\\n")) return false;
        break;
      case '\r':
        if (!jb_append(builder, "\\r")) return false;
        break;
      case '\t':
        if (!jb_append(builder, "\\t")) return false;
        break;
      default:
        if (*cursor < 0x20) {
          if (!jb_appendf(builder, "\\u%04x", *cursor)) return false;
        } else {
          char value[2] = {(char)*cursor, '\0'};
          if (!jb_append(builder, value)) return false;
        }
    }
  }
  return jb_append(builder, "\"");
}

static char *json_error(const char *path, const char *code, const char *message) {
  JsonBuilder builder;
  if (!jb_init(&builder)) {
    return NULL;
  }
  jb_append(&builder, "{\"ok\":false,\"path\":");
  jb_append_json_string(&builder, path == NULL ? "" : path);
  jb_append(&builder, ",\"code\":");
  jb_append_json_string(&builder, code);
  jb_append(&builder, ",\"message\":");
  jb_append_json_string(&builder, message);
  jb_append(&builder, "}");
  return builder.data;
}

static uint16_t read_u16_be_bytes(const unsigned char *bytes) {
  return (uint16_t)((bytes[0] << 8) | bytes[1]);
}

static uint16_t read_u16_le_bytes(const unsigned char *bytes) {
  return (uint16_t)(bytes[0] | (bytes[1] << 8));
}

static uint32_t read_u24_le_bytes(const unsigned char *bytes) {
  return (uint32_t)(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16));
}

static uint32_t read_u32_be_bytes(const unsigned char *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static uint32_t read_u32_le_bytes(const unsigned char *bytes) {
  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint16_t read_u16_tiff_bytes(const unsigned char *bytes,
                                    bool little_endian) {
  return little_endian ? read_u16_le_bytes(bytes) : read_u16_be_bytes(bytes);
}

static uint32_t read_u32_tiff_bytes(const unsigned char *bytes,
                                    bool little_endian) {
  return little_endian ? read_u32_le_bytes(bytes) : read_u32_be_bytes(bytes);
}

static uint64_t read_u64_be_bytes(const unsigned char *bytes) {
  uint64_t value = 0;
  for (int i = 0; i < 8; i++) {
    value = (value << 8) | bytes[i];
  }
  return value;
}

static bool read_at(FILE *file, uint64_t offset, void *buffer, size_t length) {
  if (fseek(file, (long)offset, SEEK_SET) != 0) {
    return false;
  }
  return fread(buffer, 1, length, file) == length;
}

static uint32_t read_u32_be_at(FILE *file, uint64_t offset) {
  unsigned char bytes[4];
  if (!read_at(file, offset, bytes, sizeof(bytes))) {
    return 0;
  }
  return read_u32_be_bytes(bytes);
}

static uint64_t read_u64_be_at(FILE *file, uint64_t offset) {
  unsigned char bytes[8];
  if (!read_at(file, offset, bytes, sizeof(bytes))) {
    return 0;
  }
  return read_u64_be_bytes(bytes);
}

static bool is_box_type(const char type[5], const char *expected) {
  return memcmp(type, expected, 4) == 0;
}

static void parse_exif(FILE *file, uint64_t payload_offset,
                       uint16_t payload_length, ImageInfo *info) {
  if (payload_length < 14) {
    return;
  }
  unsigned char prefix[14];
  if (!read_at(file, payload_offset, prefix, sizeof(prefix)) ||
      memcmp(prefix, "Exif\0\0", 6) != 0) {
    return;
  }
  bool little_endian;
  if (prefix[6] == 'I' && prefix[7] == 'I') {
    little_endian = true;
  } else if (prefix[6] == 'M' && prefix[7] == 'M') {
    little_endian = false;
  } else {
    return;
  }
  if (read_u16_tiff_bytes(prefix + 8, little_endian) != 42) {
    return;
  }
  uint64_t tiff_offset = payload_offset + 6;
  uint32_t ifd_offset = read_u32_tiff_bytes(prefix + 10, little_endian);
  uint64_t ifd_position = tiff_offset + ifd_offset;
  uint64_t payload_end = payload_offset + payload_length;
  if (ifd_position + 2 > payload_end) {
    return;
  }
  unsigned char entry_count_bytes[2];
  if (!read_at(file, ifd_position, entry_count_bytes, sizeof(entry_count_bytes))) {
    return;
  }
  uint16_t entry_count =
      read_u16_tiff_bytes(entry_count_bytes, little_endian);
  info->has_exif = true;
  for (uint16_t i = 0; i < entry_count; i++) {
    uint64_t entry_position = ifd_position + 2 + ((uint64_t)i * 12);
    if (entry_position + 12 > payload_end) {
      return;
    }
    unsigned char entry[12];
    if (!read_at(file, entry_position, entry, sizeof(entry))) {
      return;
    }
    uint16_t tag = read_u16_tiff_bytes(entry, little_endian);
    uint16_t type = read_u16_tiff_bytes(entry + 2, little_endian);
    uint32_t count = read_u32_tiff_bytes(entry + 4, little_endian);
    if (tag == 0x0112 && type == 3 && count == 1) {
      info->exif_orientation =
          (int)read_u16_tiff_bytes(entry + 8, little_endian);
      return;
    }
  }
}

static bool parse_png(FILE *file, ImageInfo *info) {
  unsigned char header[24];
  if (!read_at(file, 0, header, sizeof(header))) {
    return false;
  }
  const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a,
                                      '\n'};
  if (memcmp(header, signature, sizeof(signature)) != 0 ||
      memcmp(header + 12, "IHDR", 4) != 0) {
    return false;
  }
  info->width = (int)read_u32_be_bytes(header + 16);
  info->height = (int)read_u32_be_bytes(header + 20);
  info->format = "png";
  info->codec = "png";
  info->container = "PNG";
  return true;
}

static bool parse_gif(FILE *file, ImageInfo *info) {
  unsigned char header[10];
  if (!read_at(file, 0, header, sizeof(header))) {
    return false;
  }
  if (memcmp(header, "GIF87a", 6) != 0 && memcmp(header, "GIF89a", 6) != 0) {
    return false;
  }
  info->width = (int)read_u16_le_bytes(header + 6);
  info->height = (int)read_u16_le_bytes(header + 8);
  info->format = "gif";
  info->codec = "gif";
  info->container = "GIF";
  return true;
}

static bool parse_bmp(FILE *file, ImageInfo *info) {
  unsigned char header[26];
  if (!read_at(file, 0, header, sizeof(header)) || header[0] != 'B' ||
      header[1] != 'M') {
    return false;
  }
  int width = (int)read_u32_le_bytes(header + 18);
  int height = (int)read_u32_le_bytes(header + 22);
  if (height < 0) {
    height = -height;
  }
  info->width = width;
  info->height = height;
  info->format = "bmp";
  info->codec = "bmp";
  info->container = "BMP";
  return true;
}

static bool parse_jpeg(FILE *file, ImageInfo *info) {
  unsigned char marker[2];
  if (!read_at(file, 0, marker, sizeof(marker)) || marker[0] != 0xff ||
      marker[1] != 0xd8) {
    return false;
  }
  uint64_t offset = 2;
  for (int guard = 0; guard < 512; guard++) {
    unsigned char prefix;
    if (!read_at(file, offset, &prefix, 1)) {
      return false;
    }
    while (prefix != 0xff) {
      offset++;
      if (!read_at(file, offset, &prefix, 1)) {
        return false;
      }
    }
    unsigned char code;
    if (!read_at(file, offset + 1, &code, 1)) {
      return false;
    }
    offset += 2;
    if (code == 0xd9 || code == 0xda) {
      return false;
    }
    if (code >= 0xd0 && code <= 0xd7) {
      continue;
    }
    unsigned char length_bytes[2];
    if (!read_at(file, offset, length_bytes, sizeof(length_bytes))) {
      return false;
    }
    uint16_t segment_length = read_u16_be_bytes(length_bytes);
    if (segment_length < 2) {
      return false;
    }
    if (code == 0xe1) {
      parse_exif(file, offset + 2, segment_length - 2, info);
    }
    if ((code >= 0xc0 && code <= 0xc3) || (code >= 0xc5 && code <= 0xc7) ||
        (code >= 0xc9 && code <= 0xcb) || (code >= 0xcd && code <= 0xcf)) {
      unsigned char sof[5];
      if (!read_at(file, offset + 3, sof, sizeof(sof))) {
        return false;
      }
      info->height = (int)read_u16_be_bytes(sof);
      info->width = (int)read_u16_be_bytes(sof + 2);
      info->format = "jpeg";
      info->codec = "jpeg";
      info->container = "JPEG";
      return true;
    }
    offset += segment_length;
  }
  return false;
}

static bool parse_webp(FILE *file, ImageInfo *info) {
  unsigned char header[12];
  if (!read_at(file, 0, header, sizeof(header)) ||
      memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WEBP", 4) != 0) {
    return false;
  }
  uint64_t offset = 12;
  for (int guard = 0; guard < 64; guard++) {
    unsigned char chunk_header[8];
    if (!read_at(file, offset, chunk_header, sizeof(chunk_header))) {
      return false;
    }
    uint32_t chunk_size = read_u32_le_bytes(chunk_header + 4);
    if (memcmp(chunk_header, "VP8X", 4) == 0 && chunk_size >= 10) {
      unsigned char payload[10];
      if (!read_at(file, offset + 8, payload, sizeof(payload))) {
        return false;
      }
      info->width = (int)read_u24_le_bytes(payload + 4) + 1;
      info->height = (int)read_u24_le_bytes(payload + 7) + 1;
    } else if (memcmp(chunk_header, "VP8L", 4) == 0 && chunk_size >= 5) {
      unsigned char payload[5];
      if (!read_at(file, offset + 8, payload, sizeof(payload)) ||
          payload[0] != 0x2f) {
        return false;
      }
      info->width = 1 + (((payload[2] & 0x3f) << 8) | payload[1]);
      info->height = 1 + (((payload[4] & 0x0f) << 10) |
                          (payload[3] << 2) | ((payload[2] & 0xc0) >> 6));
    } else if (memcmp(chunk_header, "VP8 ", 4) == 0 && chunk_size >= 10) {
      unsigned char payload[10];
      if (!read_at(file, offset + 8, payload, sizeof(payload)) ||
          payload[3] != 0x9d || payload[4] != 0x01 || payload[5] != 0x2a) {
        return false;
      }
      info->width = read_u16_le_bytes(payload + 6) & 0x3fff;
      info->height = read_u16_le_bytes(payload + 8) & 0x3fff;
    }
    if (info->width > 0 && info->height > 0) {
      info->format = "webp";
      info->codec = "webp";
      info->container = "WebP";
      return true;
    }
    offset += 8 + chunk_size + (chunk_size % 2);
  }
  return false;
}

static bool parse_wav(FILE *file, int64_t file_size, AudioInfo *info) {
  unsigned char header[12];
  if (!read_at(file, 0, header, sizeof(header)) ||
      memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
    return false;
  }
  uint16_t audio_format = 0;
  uint16_t bits_per_sample = 0;
  uint32_t byte_rate = 0;
  uint32_t data_size = 0;
  uint64_t offset = 12;
  while (offset + 8 <= (uint64_t)file_size) {
    unsigned char chunk_header[8];
    if (!read_at(file, offset, chunk_header, sizeof(chunk_header))) {
      break;
    }
    uint32_t chunk_size = read_u32_le_bytes(chunk_header + 4);
    if (memcmp(chunk_header, "fmt ", 4) == 0 && chunk_size >= 16) {
      unsigned char fmt[16];
      if (!read_at(file, offset + 8, fmt, sizeof(fmt))) {
        return false;
      }
      audio_format = read_u16_le_bytes(fmt);
      info->channels = (int)read_u16_le_bytes(fmt + 2);
      info->sample_rate = (int)read_u32_le_bytes(fmt + 4);
      byte_rate = read_u32_le_bytes(fmt + 8);
      bits_per_sample = read_u16_le_bytes(fmt + 14);
    } else if (memcmp(chunk_header, "data", 4) == 0) {
      data_size = chunk_size;
    }
    offset += 8 + chunk_size + (chunk_size % 2);
  }
  if (info->sample_rate <= 0 || info->channels <= 0) {
    return false;
  }
  info->duration_micros =
      byte_rate == 0 ? 0 : (int64_t)((double)data_size * 1000000.0 / byte_rate);
  info->bitrate = byte_rate == 0 ? 0 : (int)byte_rate * 8;
  snprintf(info->codec, sizeof(info->codec), audio_format == 1 ? "pcm" : "wav");
  (void)bits_per_sample;
  info->format = "wave";
  info->container = "WAVE";
  return true;
}

static void parse_mvhd(FILE *file, uint64_t content, uint64_t size,
                       uint32_t *timescale, uint64_t *duration) {
  if (size < 24) {
    return;
  }
  unsigned char version;
  if (!read_at(file, content, &version, 1)) {
    return;
  }
  if (version == 1 && size >= 36) {
    *timescale = read_u32_be_at(file, content + 20);
    *duration = read_u64_be_at(file, content + 24);
  } else {
    *timescale = read_u32_be_at(file, content + 12);
    *duration = read_u32_be_at(file, content + 16);
  }
}

static void parse_tkhd(FILE *file, uint64_t content, uint64_t size,
                       Mp4Track *track) {
  if (size < 84) {
    return;
  }
  unsigned char version;
  if (!read_at(file, content, &version, 1)) {
    return;
  }
  uint64_t wh_offset = content + (version == 1 ? 88 : 76);
  if (wh_offset + 8 > content + size) {
    return;
  }
  uint32_t raw_width = read_u32_be_at(file, wh_offset);
  uint32_t raw_height = read_u32_be_at(file, wh_offset + 4);
  track->width = (int)(raw_width >> 16);
  track->height = (int)(raw_height >> 16);
}

static void parse_hdlr(FILE *file, uint64_t content, uint64_t size,
                       Mp4Track *track) {
  if (size < 12) {
    return;
  }
  unsigned char handler[4];
  if (!read_at(file, content + 8, handler, sizeof(handler))) {
    return;
  }
  memcpy(track->handler, handler, 4);
  track->handler[4] = '\0';
}

static void parse_stsd(FILE *file, uint64_t content, uint64_t size,
                       Mp4Track *track) {
  if (size < 16) {
    return;
  }
  uint64_t entry = content + 8;
  if (entry + 36 > content + size) {
    return;
  }
  unsigned char type[4];
  if (!read_at(file, entry + 4, type, sizeof(type))) {
    return;
  }
  memcpy(track->codec, type, 4);
  track->codec[4] = '\0';
  if (is_box_type(track->handler, "soun") && entry + 36 <= content + size) {
    unsigned char audio_fields[12];
    if (read_at(file, entry + 24, audio_fields, sizeof(audio_fields))) {
      track->channels = (int)read_u16_be_bytes(audio_fields);
      track->sample_rate = (int)(read_u32_be_bytes(audio_fields + 8) >> 16);
    }
  } else if (is_box_type(track->handler, "vide") && track->width == 0 &&
             entry + 36 <= content + size) {
    unsigned char video_fields[4];
    if (read_at(file, entry + 32, video_fields, sizeof(video_fields))) {
      track->width = (int)read_u16_be_bytes(video_fields);
      track->height = (int)read_u16_be_bytes(video_fields + 2);
    }
  }
}

static void parse_stts(FILE *file, uint64_t content, uint64_t size,
                       Mp4Track *track) {
  if (size < 16) {
    return;
  }
  uint32_t entry_count = read_u32_be_at(file, content + 4);
  uint64_t offset = content + 8;
  uint64_t end = content + size;
  for (uint32_t i = 0; i < entry_count && offset + 8 <= end; i++) {
    uint32_t sample_count = read_u32_be_at(file, offset);
    track->sample_count += sample_count;
    offset += 8;
  }
}

static void scan_minf(FILE *file, uint64_t start, uint64_t end,
                      Mp4Track *track);

static void scan_stbl(FILE *file, uint64_t start, uint64_t end,
                      Mp4Track *track) {
  uint64_t offset = start;
  while (offset + 8 <= end) {
    uint32_t size32 = read_u32_be_at(file, offset);
    unsigned char type_bytes[4];
    if (!read_at(file, offset + 4, type_bytes, sizeof(type_bytes)) ||
        size32 < 8) {
      return;
    }
    char type[5] = {(char)type_bytes[0], (char)type_bytes[1],
                    (char)type_bytes[2], (char)type_bytes[3], '\0'};
    uint64_t box_size = size32;
    uint64_t header_size = 8;
    if (size32 == 1) {
      box_size = read_u64_be_at(file, offset + 8);
      header_size = 16;
    }
    if (offset + box_size > end || box_size < header_size) {
      return;
    }
    if (is_box_type(type, "stsd")) {
      parse_stsd(file, offset + header_size, box_size - header_size, track);
    } else if (is_box_type(type, "stts")) {
      parse_stts(file, offset + header_size, box_size - header_size, track);
    }
    offset += box_size;
  }
}

static void scan_minf(FILE *file, uint64_t start, uint64_t end,
                      Mp4Track *track) {
  uint64_t offset = start;
  while (offset + 8 <= end) {
    uint32_t size32 = read_u32_be_at(file, offset);
    unsigned char type_bytes[4];
    if (!read_at(file, offset + 4, type_bytes, sizeof(type_bytes)) ||
        size32 < 8) {
      return;
    }
    char type[5] = {(char)type_bytes[0], (char)type_bytes[1],
                    (char)type_bytes[2], (char)type_bytes[3], '\0'};
    uint64_t box_size = size32;
    uint64_t header_size = 8;
    if (size32 == 1) {
      box_size = read_u64_be_at(file, offset + 8);
      header_size = 16;
    }
    if (offset + box_size > end || box_size < header_size) {
      return;
    }
    if (is_box_type(type, "stbl")) {
      scan_stbl(file, offset + header_size, offset + box_size, track);
    }
    offset += box_size;
  }
}

static void scan_mdia(FILE *file, uint64_t start, uint64_t end,
                      Mp4Track *track) {
  uint64_t offset = start;
  while (offset + 8 <= end) {
    uint32_t size32 = read_u32_be_at(file, offset);
    unsigned char type_bytes[4];
    if (!read_at(file, offset + 4, type_bytes, sizeof(type_bytes)) ||
        size32 < 8) {
      return;
    }
    char type[5] = {(char)type_bytes[0], (char)type_bytes[1],
                    (char)type_bytes[2], (char)type_bytes[3], '\0'};
    uint64_t box_size = size32;
    uint64_t header_size = 8;
    if (size32 == 1) {
      box_size = read_u64_be_at(file, offset + 8);
      header_size = 16;
    }
    if (offset + box_size > end || box_size < header_size) {
      return;
    }
    if (is_box_type(type, "hdlr")) {
      parse_hdlr(file, offset + header_size, box_size - header_size, track);
    } else if (is_box_type(type, "minf")) {
      scan_minf(file, offset + header_size, offset + box_size, track);
    }
    offset += box_size;
  }
}

static Mp4Track scan_trak(FILE *file, uint64_t start, uint64_t end) {
  Mp4Track track;
  memset(&track, 0, sizeof(track));
  uint64_t offset = start;
  while (offset + 8 <= end) {
    uint32_t size32 = read_u32_be_at(file, offset);
    unsigned char type_bytes[4];
    if (!read_at(file, offset + 4, type_bytes, sizeof(type_bytes)) ||
        size32 < 8) {
      break;
    }
    char type[5] = {(char)type_bytes[0], (char)type_bytes[1],
                    (char)type_bytes[2], (char)type_bytes[3], '\0'};
    uint64_t box_size = size32;
    uint64_t header_size = 8;
    if (size32 == 1) {
      box_size = read_u64_be_at(file, offset + 8);
      header_size = 16;
    }
    if (offset + box_size > end || box_size < header_size) {
      break;
    }
    if (is_box_type(type, "tkhd")) {
      parse_tkhd(file, offset + header_size, box_size - header_size, &track);
    } else if (is_box_type(type, "mdia")) {
      scan_mdia(file, offset + header_size, offset + box_size, &track);
    }
    offset += box_size;
  }
  return track;
}

static bool parse_mp4(FILE *file, int64_t file_size, VideoInfo *video,
                      AudioInfo *audio, bool *is_audio, bool *is_video) {
  char brand[5] = {0};
  uint64_t moov_start = 0;
  uint64_t moov_end = 0;
  uint64_t offset = 0;
  uint32_t timescale = 0;
  uint64_t duration = 0;
  while (offset + 8 <= (uint64_t)file_size) {
    uint32_t size32 = read_u32_be_at(file, offset);
    unsigned char type_bytes[4];
    if (!read_at(file, offset + 4, type_bytes, sizeof(type_bytes)) ||
        size32 < 8) {
      break;
    }
    char type[5] = {(char)type_bytes[0], (char)type_bytes[1],
                    (char)type_bytes[2], (char)type_bytes[3], '\0'};
    uint64_t box_size = size32;
    uint64_t header_size = 8;
    if (size32 == 1) {
      box_size = read_u64_be_at(file, offset + 8);
      header_size = 16;
    }
    if (offset + box_size > (uint64_t)file_size || box_size < header_size) {
      break;
    }
    if (is_box_type(type, "ftyp")) {
      unsigned char brand_bytes[4];
      if (read_at(file, offset + header_size, brand_bytes, sizeof(brand_bytes))) {
        memcpy(brand, brand_bytes, 4);
      }
    } else if (is_box_type(type, "moov")) {
      moov_start = offset + header_size;
      moov_end = offset + box_size;
    }
    offset += box_size;
  }
  if (moov_start == 0 || brand[0] == '\0') {
    return false;
  }
  offset = moov_start;
  while (offset + 8 <= moov_end) {
    uint32_t size32 = read_u32_be_at(file, offset);
    unsigned char type_bytes[4];
    if (!read_at(file, offset + 4, type_bytes, sizeof(type_bytes)) ||
        size32 < 8) {
      break;
    }
    char type[5] = {(char)type_bytes[0], (char)type_bytes[1],
                    (char)type_bytes[2], (char)type_bytes[3], '\0'};
    uint64_t box_size = size32;
    uint64_t header_size = 8;
    if (size32 == 1) {
      box_size = read_u64_be_at(file, offset + 8);
      header_size = 16;
    }
    if (offset + box_size > moov_end || box_size < header_size) {
      break;
    }
    if (is_box_type(type, "mvhd")) {
      parse_mvhd(file, offset + header_size, box_size - header_size, &timescale,
                 &duration);
    } else if (is_box_type(type, "trak")) {
      Mp4Track track = scan_trak(file, offset + header_size, offset + box_size);
      if (is_box_type(track.handler, "vide") && track.width > 0 &&
          track.height > 0) {
        video->width = track.width;
        video->height = track.height;
        video->frame_rate =
            duration == 0 ? 0.0 : (double)track.sample_count *
                                    (double)timescale / (double)duration;
        snprintf(video->codec, sizeof(video->codec), "%s", track.codec);
        *is_video = true;
      } else if (is_box_type(track.handler, "soun")) {
        audio->sample_rate = track.sample_rate;
        audio->channels = track.channels;
        snprintf(audio->codec, sizeof(audio->codec), "%s", track.codec);
        *is_audio = true;
      }
    }
    offset += box_size;
  }
  int64_t duration_micros =
      timescale == 0 ? 0 : (int64_t)((double)duration * 1000000.0 / timescale);
  bool quicktime = memcmp(brand, "qt  ", 4) == 0;
  if (*is_video) {
    video->duration_micros = duration_micros;
    video->bitrate = duration_micros == 0
                         ? 0
                         : (int)((double)file_size * 8.0 * 1000000.0 /
                                 (double)duration_micros);
    video->format = quicktime ? "quicktime" : "mp4";
    video->container = quicktime ? "QuickTime" : "MP4";
  }
  if (*is_audio) {
    audio->duration_micros = duration_micros;
    audio->bitrate = duration_micros == 0
                         ? 0
                         : (int)((double)file_size * 8.0 * 1000000.0 /
                                 (double)duration_micros);
    audio->format = quicktime ? "mp4" : "aac";
    audio->container = quicktime ? "QuickTime" : "MP4";
  }
  return *is_video || *is_audio;
}

static void append_common(JsonBuilder *builder, const char *kind,
                          const char *path, int64_t size_bytes,
                          const char *format, const char *container) {
  jb_append(builder, "{\"ok\":true,\"kind\":");
  jb_append_json_string(builder, kind);
  jb_append(builder, ",\"path\":");
  jb_append_json_string(builder, path);
  jb_appendf(builder, ",\"fileSize\":%lld", (long long)size_bytes);
  jb_append(builder, ",\"format\":");
  jb_append_json_string(builder, format);
  jb_append(builder, ",\"metadata\":{\"container\":");
  jb_append_json_string(builder, container);
  jb_append(builder, "}");
}

static char *json_image(const char *path, int64_t size_bytes,
                        const ImageInfo *info) {
  JsonBuilder builder;
  if (!jb_init(&builder)) {
    return NULL;
  }
  append_common(&builder, "image", path, size_bytes, info->format,
                info->container);
  jb_appendf(&builder, ",\"width\":%d,\"height\":%d", info->width,
             info->height);
  jb_append(&builder, ",\"codec\":");
  jb_append_json_string(&builder, info->codec);
  jb_append(&builder, ",\"bitrate\":null,\"exif\":{");
  if (info->has_exif) {
    jb_append(&builder, "\"present\":\"true\"");
    if (info->exif_orientation > 0) {
      jb_appendf(&builder, ",\"orientation\":\"%d\"", info->exif_orientation);
    }
  }
  jb_append(&builder, "}}");
  return builder.data;
}

static char *json_video(const char *path, int64_t size_bytes,
                        const VideoInfo *info) {
  JsonBuilder builder;
  if (!jb_init(&builder)) {
    return NULL;
  }
  append_common(&builder, "video", path, size_bytes, info->format,
                info->container);
  jb_appendf(&builder,
             ",\"width\":%d,\"height\":%d,\"durationMicros\":%lld",
             info->width, info->height, (long long)info->duration_micros);
  if (info->frame_rate > 0.0) {
    jb_appendf(&builder, ",\"frameRate\":%.6f,\"codec\":", info->frame_rate);
  } else {
    jb_append(&builder, ",\"frameRate\":null,\"codec\":");
  }
  jb_append_json_string(&builder, info->codec[0] == '\0' ? "unknown" : info->codec);
  if (info->bitrate > 0) {
    jb_appendf(&builder, ",\"bitrate\":%d}", info->bitrate);
  } else {
    jb_append(&builder, ",\"bitrate\":null}");
  }
  return builder.data;
}

static char *json_audio(const char *path, int64_t size_bytes,
                        const AudioInfo *info) {
  JsonBuilder builder;
  if (!jb_init(&builder)) {
    return NULL;
  }
  append_common(&builder, "audio", path, size_bytes, info->format,
                info->container);
  jb_appendf(&builder, ",\"durationMicros\":%lld",
             (long long)info->duration_micros);
  if (info->sample_rate > 0) {
    jb_appendf(&builder, ",\"sampleRate\":%d", info->sample_rate);
  } else {
    jb_append(&builder, ",\"sampleRate\":null");
  }
  if (info->channels > 0) {
    jb_appendf(&builder, ",\"channels\":%d", info->channels);
  } else {
    jb_append(&builder, ",\"channels\":null");
  }
  jb_append(&builder, ",\"codec\":");
  jb_append_json_string(&builder, info->codec[0] == '\0' ? "unknown" : info->codec);
  if (info->bitrate > 0) {
    jb_appendf(&builder, ",\"bitrate\":%d}", info->bitrate);
  } else {
    jb_append(&builder, ",\"bitrate\":null}");
  }
  return builder.data;
}

char *fmt_probe_media_file(const char *path) {
  if (path == NULL || path[0] == '\0') {
    return json_error(path, "not_found", "Path must not be empty.");
  }

  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    return json_error(path, "not_found", "File does not exist or is not readable.");
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return json_error(path, "not_readable", "Could not read file size.");
  }
  long size_long = ftell(file);
  if (size_long < 0) {
    fclose(file);
    return json_error(path, "not_readable", "Could not read file size.");
  }
  int64_t size_bytes = (int64_t)size_long;
  rewind(file);

  ImageInfo image;
  memset(&image, 0, sizeof(image));
  if (parse_png(file, &image) || parse_jpeg(file, &image) ||
      parse_webp(file, &image) || parse_gif(file, &image) ||
      parse_bmp(file, &image)) {
    fclose(file);
    return json_image(path, size_bytes, &image);
  }

  AudioInfo audio;
  memset(&audio, 0, sizeof(audio));
  if (parse_wav(file, size_bytes, &audio)) {
    fclose(file);
    return json_audio(path, size_bytes, &audio);
  }

  VideoInfo video;
  memset(&video, 0, sizeof(video));
  bool is_audio = false;
  bool is_video = false;
  if (parse_mp4(file, size_bytes, &video, &audio, &is_audio, &is_video)) {
    fclose(file);
    if (is_video) {
      return json_video(path, size_bytes, &video);
    }
    return json_audio(path, size_bytes, &audio);
  }

  fclose(file);
  return json_error(path, "unsupported",
                    "File is not a supported image, video, or audio file.");
}

void fmt_free_string(char *value) { free(value); }
