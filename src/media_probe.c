#include "media_probe.h"
#include "media_metadata.h"

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
  MetadataMap metadata;
  MetadataMap exif;
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
  MetadataMap metadata;
} VideoInfo;

typedef struct {
  int64_t duration_micros;
  int sample_rate;
  int channels;
  int bitrate;
  char codec[8];
  const char *format;
  const char *container;
  MetadataMap metadata;
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

typedef struct {
  uint64_t offset;
  uint64_t size;
  uint64_t header_size;
  char type[5];
} Mp4Box;

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

static bool append_metadata_json(JsonBuilder *builder,
                                 const MetadataMap *metadata) {
  jb_append(builder, "{");
  for (size_t i = 0; i < metadata->count; i++) {
    if (i > 0) {
      jb_append(builder, ",");
    }
    jb_append_json_string(builder, metadata->entries[i].key);
    jb_append(builder, ":");
    jb_append_json_string(builder, metadata->entries[i].value);
  }
  return jb_append(builder, "}");
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

static bool read_mp4_box(FILE *file, uint64_t offset, uint64_t end,
                         Mp4Box *box) {
  if (offset + 8 > end) {
    return false;
  }
  uint32_t size32 = read_u32_be_at(file, offset);
  unsigned char type_bytes[4];
  if (!read_at(file, offset + 4, type_bytes, sizeof(type_bytes)) ||
      size32 < 8) {
    return false;
  }
  uint64_t box_size = size32;
  uint64_t header_size = 8;
  if (size32 == 1) {
    if (offset + 16 > end) {
      return false;
    }
    box_size = read_u64_be_at(file, offset + 8);
    header_size = 16;
  }
  if (box_size == 0) {
    box_size = end - offset;
  }
  if (box_size < header_size || offset + box_size > end) {
    return false;
  }
  box->offset = offset;
  box->size = box_size;
  box->header_size = header_size;
  memcpy(box->type, type_bytes, 4);
  box->type[4] = '\0';
  return true;
}

static const char *exif_tag_name(uint16_t tag) {
  switch (tag) {
    case 0x010e:
      return "imageDescription";
    case 0x010f:
      return "make";
    case 0x0110:
      return "model";
    case 0x0112:
      return "orientation";
    case 0x011a:
      return "xResolution";
    case 0x011b:
      return "yResolution";
    case 0x0128:
      return "resolutionUnit";
    case 0x0131:
      return "software";
    case 0x0132:
      return "dateTime";
    case 0x013b:
      return "artist";
    case 0x0213:
      return "yCbCrPositioning";
    case 0x8298:
      return "copyright";
    case 0x829a:
      return "exposureTime";
    case 0x829d:
      return "fNumber";
    case 0x8822:
      return "exposureProgram";
    case 0x8827:
      return "photographicSensitivity";
    case 0x9000:
      return "exifVersion";
    case 0x9003:
      return "dateTimeOriginal";
    case 0x9004:
      return "dateTimeDigitized";
    case 0x9010:
      return "offsetTime";
    case 0x9011:
      return "offsetTimeOriginal";
    case 0x9012:
      return "offsetTimeDigitized";
    case 0x9201:
      return "shutterSpeedValue";
    case 0x9202:
      return "apertureValue";
    case 0x9204:
      return "exposureBiasValue";
    case 0x9207:
      return "meteringMode";
    case 0x9208:
      return "lightSource";
    case 0x9209:
      return "flash";
    case 0x920a:
      return "focalLength";
    case 0x9290:
      return "subSecTime";
    case 0x9291:
      return "subSecTimeOriginal";
    case 0x9292:
      return "subSecTimeDigitized";
    case 0xa001:
      return "colorSpace";
    case 0xa002:
      return "pixelXDimension";
    case 0xa003:
      return "pixelYDimension";
    case 0xa20e:
      return "focalPlaneXResolution";
    case 0xa20f:
      return "focalPlaneYResolution";
    case 0xa210:
      return "focalPlaneResolutionUnit";
    case 0xa217:
      return "sensingMethod";
    case 0xa300:
      return "fileSource";
    case 0xa301:
      return "sceneType";
    case 0xa402:
      return "exposureMode";
    case 0xa403:
      return "whiteBalance";
    case 0xa404:
      return "digitalZoomRatio";
    case 0xa405:
      return "focalLengthIn35mmFilm";
    case 0xa406:
      return "sceneCaptureType";
    case 0xa408:
      return "contrast";
    case 0xa409:
      return "saturation";
    case 0xa40a:
      return "sharpness";
    case 0xa431:
      return "bodySerialNumber";
    case 0xa432:
      return "lensSpecification";
    case 0xa433:
      return "lensMake";
    case 0xa434:
      return "lensModel";
    case 0x0000:
      return "gpsVersionID";
    case 0x0001:
      return "gpsLatitudeRef";
    case 0x0002:
      return "gpsLatitude";
    case 0x0003:
      return "gpsLongitudeRef";
    case 0x0004:
      return "gpsLongitude";
    case 0x0005:
      return "gpsAltitudeRef";
    case 0x0006:
      return "gpsAltitude";
    case 0x0007:
      return "gpsTimeStamp";
    case 0x001d:
      return "gpsDateStamp";
    default:
      return NULL;
  }
}

static size_t tiff_type_size(uint16_t type) {
  switch (type) {
    case 1:
    case 2:
    case 6:
    case 7:
      return 1;
    case 3:
    case 8:
      return 2;
    case 4:
    case 9:
      return 4;
    case 5:
    case 10:
      return 8;
    default:
      return 0;
  }
}

static int32_t read_i32_tiff_bytes(const unsigned char *bytes,
                                   bool little_endian) {
  return (int32_t)read_u32_tiff_bytes(bytes, little_endian);
}

static void append_to_buffer(char *buffer, size_t buffer_size,
                             const char *text) {
  size_t length = strlen(buffer);
  if (length + 1 >= buffer_size) {
    return;
  }
  snprintf(buffer + length, buffer_size - length, "%s", text);
}

static void copy_capped_local(char *destination, size_t destination_size,
                              const char *source) {
  if (destination_size == 0) {
    return;
  }
  if (source == NULL) {
    destination[0] = '\0';
    return;
  }
  snprintf(destination, destination_size, "%s", source);
}

static void copy_sanitized_bytes_local(char *destination,
                                       size_t destination_size,
                                       const unsigned char *source,
                                       size_t length) {
  if (destination_size == 0) {
    return;
  }
  size_t start = 0;
  while (start < length &&
         (source[start] == '\0' || source[start] == ' ' ||
          source[start] == '\t' || source[start] == '\r' ||
          source[start] == '\n')) {
    start++;
  }
  size_t end = length;
  while (end > start &&
         (source[end - 1] == '\0' || source[end - 1] == ' ' ||
          source[end - 1] == '\t' || source[end - 1] == '\r' ||
          source[end - 1] == '\n')) {
    end--;
  }
  size_t written = 0;
  for (size_t i = start; i < end && written + 1 < destination_size; i++) {
    unsigned char value = source[i];
    if (value == '\0') {
      continue;
    }
    if (value == '\r' || value == '\n' || value == '\t') {
      value = ' ';
    } else if (value < 0x20) {
      continue;
    }
    destination[written++] = (char)value;
  }
  destination[written] = '\0';
}

static bool format_tiff_value(const unsigned char *data, uint16_t type,
                              uint32_t count, bool little_endian,
                              char *output, size_t output_size) {
  if (output_size == 0) {
    return false;
  }
  output[0] = '\0';
  if (data == NULL || count == 0) {
    return false;
  }
  if (type == 2) {
    copy_sanitized_bytes_local(output, output_size, data, count);
    return output[0] != '\0';
  }
  if ((type == 7 || type == 1) && count >= 4) {
    bool printable = true;
    for (uint32_t i = 0; i < count; i++) {
      if (data[i] == '\0') {
        continue;
      }
      if (data[i] < 0x20 || data[i] > 0x7e) {
        printable = false;
        break;
      }
    }
    if (printable) {
      copy_sanitized_bytes_local(output, output_size, data, count);
      return output[0] != '\0';
    }
  }
  uint32_t limit = count > 16 ? 16 : count;
  for (uint32_t i = 0; i < limit; i++) {
    char part[64];
    if (i > 0) {
      append_to_buffer(output, output_size, ",");
    }
    switch (type) {
      case 1:
      case 7:
        snprintf(part, sizeof(part), "%u", data[i]);
        break;
      case 3:
        snprintf(part, sizeof(part), "%u",
                 read_u16_tiff_bytes(data + (i * 2), little_endian));
        break;
      case 4:
        snprintf(part, sizeof(part), "%u",
                 read_u32_tiff_bytes(data + (i * 4), little_endian));
        break;
      case 5: {
        uint32_t numerator = read_u32_tiff_bytes(data + (i * 8), little_endian);
        uint32_t denominator =
            read_u32_tiff_bytes(data + (i * 8) + 4, little_endian);
        if (denominator == 0) {
          snprintf(part, sizeof(part), "%u/0", numerator);
        } else {
          snprintf(part, sizeof(part), "%u/%u", numerator, denominator);
        }
        break;
      }
      case 9:
        snprintf(part, sizeof(part), "%d",
                 read_i32_tiff_bytes(data + (i * 4), little_endian));
        break;
      case 10: {
        int32_t numerator = read_i32_tiff_bytes(data + (i * 8), little_endian);
        int32_t denominator =
            read_i32_tiff_bytes(data + (i * 8) + 4, little_endian);
        if (denominator == 0) {
          snprintf(part, sizeof(part), "%d/0", numerator);
        } else {
          snprintf(part, sizeof(part), "%d/%d", numerator, denominator);
        }
        break;
      }
      default:
        return false;
    }
    append_to_buffer(output, output_size, part);
  }
  if (count > limit) {
    append_to_buffer(output, output_size, ",...");
  }
  return output[0] != '\0';
}

static void parse_tiff_ifd(FILE *file, uint64_t tiff_offset, uint64_t payload_end,
                           uint32_t ifd_offset, bool little_endian,
                           ImageInfo *info, int depth) {
  if (depth > 4 || ifd_offset == 0) {
    return;
  }
  uint64_t ifd_position = tiff_offset + ifd_offset;
  if (ifd_position + 2 > payload_end) {
    return;
  }
  unsigned char entry_count_bytes[2];
  if (!read_at(file, ifd_position, entry_count_bytes, sizeof(entry_count_bytes))) {
    return;
  }
  uint16_t entry_count =
      read_u16_tiff_bytes(entry_count_bytes, little_endian);
  uint32_t exif_ifd_offset = 0;
  uint32_t gps_ifd_offset = 0;
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
    uint32_t value_or_offset = read_u32_tiff_bytes(entry + 8, little_endian);
    if (tag == 0x8769) {
      exif_ifd_offset = value_or_offset;
    } else if (tag == 0x8825) {
      gps_ifd_offset = value_or_offset;
    }
    size_t type_size = tiff_type_size(type);
    if (type_size == 0 || count == 0 || count > UINT32_MAX / type_size) {
      continue;
    }
    uint64_t value_size = (uint64_t)type_size * (uint64_t)count;
    uint64_t value_position =
        value_size <= 4 ? entry_position + 8 : tiff_offset + value_or_offset;
    if (value_position + value_size > payload_end) {
      continue;
    }
    unsigned char inline_value[4];
    const unsigned char *value_data = NULL;
    unsigned char *allocated = NULL;
    if (value_size <= 4) {
      memcpy(inline_value, entry + 8, sizeof(inline_value));
      value_data = inline_value;
    } else {
      allocated = (unsigned char *)malloc((size_t)value_size);
      if (allocated == NULL) {
        continue;
      }
      if (!read_at(file, value_position, allocated, (size_t)value_size)) {
        free(allocated);
        continue;
      }
      value_data = allocated;
    }
    char formatted[METADATA_VALUE_SIZE];
    if (format_tiff_value(value_data, type, count, little_endian, formatted,
                          sizeof(formatted))) {
      const char *name = exif_tag_name(tag);
      char fallback_key[METADATA_KEY_SIZE];
      if (name == NULL) {
        snprintf(fallback_key, sizeof(fallback_key), "tag.0x%04x", tag);
        name = fallback_key;
      }
      metadata_set_unique(&info->exif, name, formatted);
      if (tag == 0x0112 && type == 3 && count == 1) {
        info->exif_orientation =
            (int)read_u16_tiff_bytes(value_data, little_endian);
      }
    }
    free(allocated);
  }
  uint64_t next_ifd_position = ifd_position + 2 + ((uint64_t)entry_count * 12);
  if (next_ifd_position + 4 <= payload_end) {
    uint32_t next_ifd_offset = read_u32_be_at(file, next_ifd_position);
    if (little_endian) {
      unsigned char bytes[4];
      if (read_at(file, next_ifd_position, bytes, sizeof(bytes))) {
        next_ifd_offset = read_u32_le_bytes(bytes);
      }
    }
    parse_tiff_ifd(file, tiff_offset, payload_end, next_ifd_offset,
                   little_endian, info, depth + 1);
  }
  parse_tiff_ifd(file, tiff_offset, payload_end, exif_ifd_offset, little_endian,
                 info, depth + 1);
  parse_tiff_ifd(file, tiff_offset, payload_end, gps_ifd_offset, little_endian,
                 info, depth + 1);
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
  uint64_t payload_end = payload_offset + payload_length;
  info->has_exif = true;
  metadata_set(&info->exif, "present", "true");
  parse_tiff_ifd(file, tiff_offset, payload_end, ifd_offset, little_endian,
                 info, 0);
}

static bool parse_png(FILE *file, ImageInfo *info) {
  unsigned char header[29];
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
  metadata_set(&info->metadata, "container", info->container);
  metadata_set(&info->metadata, "codec", info->codec);
  metadata_setf(&info->metadata, "width", "%d", info->width);
  metadata_setf(&info->metadata, "height", "%d", info->height);
  metadata_setf(&info->metadata, "bitDepth", "%u", header[24]);
  metadata_setf(&info->metadata, "colorType", "%u", header[25]);
  metadata_setf(&info->metadata, "compression", "%u", header[26]);
  metadata_setf(&info->metadata, "filter", "%u", header[27]);
  metadata_setf(&info->metadata, "interlace", "%u", header[28]);
  uint64_t offset = 33;
  for (int guard = 0; guard < 512; guard++) {
    unsigned char chunk_header[8];
    if (!read_at(file, offset, chunk_header, sizeof(chunk_header))) {
      break;
    }
    uint32_t chunk_size = read_u32_be_bytes(chunk_header);
    char chunk_type[5] = {(char)chunk_header[4], (char)chunk_header[5],
                          (char)chunk_header[6], (char)chunk_header[7], '\0'};
    if (memcmp(chunk_type, "IEND", 4) == 0) {
      break;
    }
    if (chunk_size > 1024 * 1024) {
      offset += 12 + chunk_size;
      continue;
    }
    if (memcmp(chunk_type, "tEXt", 4) == 0 && chunk_size > 2) {
      unsigned char *payload = (unsigned char *)malloc(chunk_size);
      if (payload != NULL && read_at(file, offset + 8, payload, chunk_size)) {
        size_t separator = 0;
        while (separator < chunk_size && payload[separator] != '\0') {
          separator++;
        }
        if (separator > 0 && separator + 1 < chunk_size) {
          char key[METADATA_KEY_SIZE];
          char value[METADATA_VALUE_SIZE];
          copy_sanitized_bytes_local(key, sizeof(key), payload, separator);
          copy_sanitized_bytes_local(value, sizeof(value), payload + separator + 1,
                                     chunk_size - separator - 1);
          metadata_set_unique(&info->metadata, key, value);
        }
      }
      free(payload);
    } else if (memcmp(chunk_type, "pHYs", 4) == 0 && chunk_size >= 9) {
      unsigned char payload[9];
      if (read_at(file, offset + 8, payload, sizeof(payload))) {
        metadata_setf(&info->metadata, "pixelsPerUnitX", "%u",
                      read_u32_be_bytes(payload));
        metadata_setf(&info->metadata, "pixelsPerUnitY", "%u",
                      read_u32_be_bytes(payload + 4));
        metadata_setf(&info->metadata, "pixelUnit", "%u", payload[8]);
      }
    } else if (memcmp(chunk_type, "tIME", 4) == 0 && chunk_size >= 7) {
      unsigned char payload[7];
      if (read_at(file, offset + 8, payload, sizeof(payload))) {
        metadata_setf(&info->metadata, "modifiedTime",
                      "%04u-%02u-%02uT%02u:%02u:%02uZ",
                      read_u16_be_bytes(payload), payload[2], payload[3],
                      payload[4], payload[5], payload[6]);
      }
    }
    offset += 12 + chunk_size;
  }
  return true;
}

static bool parse_gif(FILE *file, ImageInfo *info) {
  unsigned char header[13];
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
  metadata_set(&info->metadata, "container", info->container);
  metadata_set(&info->metadata, "codec", info->codec);
  metadata_setf(&info->metadata, "width", "%d", info->width);
  metadata_setf(&info->metadata, "height", "%d", info->height);
  metadata_set_bytes(&info->metadata, "version", header, 6);
  metadata_setf(&info->metadata, "colorResolution", "%u",
                ((header[10] & 0x70) >> 4) + 1);
  metadata_setf(&info->metadata, "hasGlobalColorTable", "%s",
                (header[10] & 0x80) != 0 ? "true" : "false");
  metadata_setf(&info->metadata, "globalColorTableSize", "%u",
                1u << ((header[10] & 0x07) + 1));
  metadata_setf(&info->metadata, "backgroundColorIndex", "%u", header[11]);
  metadata_setf(&info->metadata, "pixelAspectRatio", "%u", header[12]);
  return true;
}

static bool parse_bmp(FILE *file, ImageInfo *info) {
  unsigned char header[54];
  if (!read_at(file, 0, header, sizeof(header)) || header[0] != 'B' ||
      header[1] != 'M') {
    return false;
  }
  uint32_t file_size = read_u32_le_bytes(header + 2);
  uint32_t data_offset = read_u32_le_bytes(header + 10);
  uint32_t dib_size = read_u32_le_bytes(header + 14);
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
  metadata_set(&info->metadata, "container", info->container);
  metadata_set(&info->metadata, "codec", info->codec);
  metadata_setf(&info->metadata, "width", "%d", info->width);
  metadata_setf(&info->metadata, "height", "%d", info->height);
  metadata_setf(&info->metadata, "declaredFileSize", "%u", file_size);
  metadata_setf(&info->metadata, "pixelDataOffset", "%u", data_offset);
  metadata_setf(&info->metadata, "dibHeaderSize", "%u", dib_size);
  metadata_setf(&info->metadata, "bitsPerPixel", "%u",
                read_u16_le_bytes(header + 28));
  metadata_setf(&info->metadata, "compression", "%u",
                read_u32_le_bytes(header + 30));
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
    } else if (code == 0xe0 && segment_length > 7) {
      unsigned char app0[14];
      size_t app0_size = segment_length - 2;
      size_t to_read = app0_size < sizeof(app0) ? app0_size : sizeof(app0);
      if (read_at(file, offset + 2, app0, to_read) && to_read >= 7 &&
          memcmp(app0, "JFIF\0", 5) == 0) {
        metadata_set(&info->metadata, "jfifVersion",
                     app0[5] == 1 && app0[6] == 2 ? "1.02" : "present");
        if (to_read >= 14) {
          metadata_setf(&info->metadata, "densityUnit", "%u", app0[7]);
          metadata_setf(&info->metadata, "xDensity", "%u",
                        read_u16_be_bytes(app0 + 8));
          metadata_setf(&info->metadata, "yDensity", "%u",
                        read_u16_be_bytes(app0 + 10));
        }
      }
    }
    if ((code >= 0xc0 && code <= 0xc3) || (code >= 0xc5 && code <= 0xc7) ||
        (code >= 0xc9 && code <= 0xcb) || (code >= 0xcd && code <= 0xcf)) {
      unsigned char sof[6];
      if (!read_at(file, offset + 2, sof, sizeof(sof))) {
        return false;
      }
      info->height = (int)read_u16_be_bytes(sof + 1);
      info->width = (int)read_u16_be_bytes(sof + 3);
      info->format = "jpeg";
      info->codec = "jpeg";
      info->container = "JPEG";
      metadata_set(&info->metadata, "container", info->container);
      metadata_set(&info->metadata, "codec", info->codec);
      metadata_setf(&info->metadata, "width", "%d", info->width);
      metadata_setf(&info->metadata, "height", "%d", info->height);
      metadata_setf(&info->metadata, "precision", "%u", sof[0]);
      metadata_setf(&info->metadata, "components", "%u", sof[5]);
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
  metadata_set(&info->metadata, "container", "WebP");
  metadata_setf(&info->metadata, "riffSize", "%u", read_u32_le_bytes(header + 4));
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
      metadata_setf(&info->metadata, "hasAnimation", "%s",
                    (payload[0] & 0x02) != 0 ? "true" : "false");
      metadata_setf(&info->metadata, "hasExif", "%s",
                    (payload[0] & 0x08) != 0 ? "true" : "false");
      metadata_setf(&info->metadata, "hasAlpha", "%s",
                    (payload[0] & 0x10) != 0 ? "true" : "false");
      metadata_setf(&info->metadata, "hasIccProfile", "%s",
                    (payload[0] & 0x20) != 0 ? "true" : "false");
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
      metadata_set(&info->metadata, "container", info->container);
      metadata_set(&info->metadata, "codec", info->codec);
      metadata_setf(&info->metadata, "width", "%d", info->width);
      metadata_setf(&info->metadata, "height", "%d", info->height);
      return true;
    }
    offset += 8 + chunk_size + (chunk_size % 2);
  }
  return false;
}

static const char *wav_info_key(const char id[5]) {
  if (memcmp(id, "IART", 4) == 0) return "artist";
  if (memcmp(id, "ICMT", 4) == 0) return "comment";
  if (memcmp(id, "ICOP", 4) == 0) return "copyright";
  if (memcmp(id, "ICRD", 4) == 0) return "creationDate";
  if (memcmp(id, "IGNR", 4) == 0) return "genre";
  if (memcmp(id, "IKEY", 4) == 0) return "keywords";
  if (memcmp(id, "INAM", 4) == 0) return "name";
  if (memcmp(id, "IPRD", 4) == 0) return "product";
  if (memcmp(id, "ISBJ", 4) == 0) return "subject";
  if (memcmp(id, "ISFT", 4) == 0) return "software";
  if (memcmp(id, "ISRC", 4) == 0) return "source";
  if (memcmp(id, "ITCH", 4) == 0) return "technician";
  return id;
}

static void parse_wav_info_list(FILE *file, uint64_t content_offset,
                                uint32_t content_size, AudioInfo *info) {
  if (content_size < 4) {
    return;
  }
  unsigned char list_type[4];
  if (!read_at(file, content_offset, list_type, sizeof(list_type)) ||
      memcmp(list_type, "INFO", 4) != 0) {
    return;
  }
  uint64_t offset = content_offset + 4;
  uint64_t end = content_offset + content_size;
  while (offset + 8 <= end) {
    unsigned char chunk_header[8];
    if (!read_at(file, offset, chunk_header, sizeof(chunk_header))) {
      return;
    }
    uint32_t chunk_size = read_u32_le_bytes(chunk_header + 4);
    if (offset + 8 + chunk_size > end) {
      return;
    }
    char key[5] = {(char)chunk_header[0], (char)chunk_header[1],
                   (char)chunk_header[2], (char)chunk_header[3], '\0'};
    if (chunk_size > 0 && chunk_size <= 1024 * 1024) {
      unsigned char *payload = (unsigned char *)malloc(chunk_size);
      if (payload != NULL && read_at(file, offset + 8, payload, chunk_size)) {
        metadata_set_unique_bytes(&info->metadata, wav_info_key(key), payload,
                                  chunk_size);
      }
      free(payload);
    }
    offset += 8 + chunk_size + (chunk_size % 2);
  }
}

static bool parse_wav(FILE *file, int64_t file_size, AudioInfo *info) {
  unsigned char header[12];
  if (!read_at(file, 0, header, sizeof(header)) ||
      memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
    return false;
  }
  metadata_set(&info->metadata, "container", "WAVE");
  metadata_setf(&info->metadata, "riffSize", "%u", read_u32_le_bytes(header + 4));
  uint16_t audio_format = 0;
  uint16_t bits_per_sample = 0;
  uint32_t byte_rate = 0;
  uint32_t data_size = 0;
  uint16_t block_align = 0;
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
      block_align = read_u16_le_bytes(fmt + 12);
      bits_per_sample = read_u16_le_bytes(fmt + 14);
    } else if (memcmp(chunk_header, "data", 4) == 0) {
      data_size = chunk_size;
    } else if (memcmp(chunk_header, "LIST", 4) == 0) {
      parse_wav_info_list(file, offset + 8, chunk_size, info);
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
  info->format = "wave";
  info->container = "WAVE";
  metadata_set(&info->metadata, "container", info->container);
  metadata_set(&info->metadata, "codec", info->codec);
  metadata_setf(&info->metadata, "audioFormat", "%u", audio_format);
  metadata_setf(&info->metadata, "sampleRate", "%d", info->sample_rate);
  metadata_setf(&info->metadata, "channels", "%d", info->channels);
  metadata_setf(&info->metadata, "byteRate", "%u", byte_rate);
  metadata_setf(&info->metadata, "blockAlign", "%u", block_align);
  metadata_setf(&info->metadata, "bitsPerSample", "%u", bits_per_sample);
  metadata_setf(&info->metadata, "dataSize", "%u", data_size);
  if (info->duration_micros > 0) {
    metadata_setf(&info->metadata, "durationMicros", "%lld",
                  (long long)info->duration_micros);
  }
  if (info->bitrate > 0) {
    metadata_setf(&info->metadata, "bitrate", "%d", info->bitrate);
  }
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
    Mp4Box box;
    if (!read_mp4_box(file, offset, end, &box)) {
      return;
    }
    if (is_box_type(box.type, "stsd")) {
      parse_stsd(file, box.offset + box.header_size, box.size - box.header_size,
                 track);
    } else if (is_box_type(box.type, "stts")) {
      parse_stts(file, box.offset + box.header_size, box.size - box.header_size,
                 track);
    }
    offset += box.size;
  }
}

static void scan_minf(FILE *file, uint64_t start, uint64_t end,
                      Mp4Track *track) {
  uint64_t offset = start;
  while (offset + 8 <= end) {
    Mp4Box box;
    if (!read_mp4_box(file, offset, end, &box)) {
      return;
    }
    if (is_box_type(box.type, "stbl")) {
      scan_stbl(file, box.offset + box.header_size, box.offset + box.size,
                track);
    }
    offset += box.size;
  }
}

static void scan_mdia(FILE *file, uint64_t start, uint64_t end,
                      Mp4Track *track) {
  uint64_t offset = start;
  while (offset + 8 <= end) {
    Mp4Box box;
    if (!read_mp4_box(file, offset, end, &box)) {
      return;
    }
    if (is_box_type(box.type, "hdlr")) {
      parse_hdlr(file, box.offset + box.header_size, box.size - box.header_size,
                 track);
    } else if (is_box_type(box.type, "minf")) {
      scan_minf(file, box.offset + box.header_size, box.offset + box.size,
                track);
    }
    offset += box.size;
  }
}

static Mp4Track scan_trak(FILE *file, uint64_t start, uint64_t end) {
  Mp4Track track;
  memset(&track, 0, sizeof(track));
  uint64_t offset = start;
  while (offset + 8 <= end) {
    Mp4Box box;
    if (!read_mp4_box(file, offset, end, &box)) {
      break;
    }
    if (is_box_type(box.type, "tkhd")) {
      parse_tkhd(file, box.offset + box.header_size, box.size - box.header_size,
                 &track);
    } else if (is_box_type(box.type, "mdia")) {
      scan_mdia(file, box.offset + box.header_size, box.offset + box.size,
                &track);
    }
    offset += box.size;
  }
  return track;
}

static const char *mp4_tag_key(const char type[5]) {
  if (memcmp(type, "\251nam", 4) == 0) return "title";
  if (memcmp(type, "\251ART", 4) == 0) return "artist";
  if (memcmp(type, "aART", 4) == 0) return "albumArtist";
  if (memcmp(type, "\251alb", 4) == 0) return "album";
  if (memcmp(type, "\251day", 4) == 0) return "date";
  if (memcmp(type, "\251gen", 4) == 0) return "genre";
  if (memcmp(type, "gnre", 4) == 0) return "genre";
  if (memcmp(type, "\251grp", 4) == 0) return "grouping";
  if (memcmp(type, "\251wrt", 4) == 0) return "composer";
  if (memcmp(type, "\251too", 4) == 0) return "encoder";
  if (memcmp(type, "\251cmt", 4) == 0) return "comment";
  if (memcmp(type, "desc", 4) == 0) return "description";
  if (memcmp(type, "ldes", 4) == 0) return "longDescription";
  if (memcmp(type, "tvsh", 4) == 0) return "show";
  if (memcmp(type, "tven", 4) == 0) return "episodeId";
  if (memcmp(type, "purd", 4) == 0) return "purchaseDate";
  if (memcmp(type, "cpil", 4) == 0) return "compilation";
  if (memcmp(type, "disk", 4) == 0) return "disc";
  if (memcmp(type, "trkn", 4) == 0) return "track";
  if (memcmp(type, "tmpo", 4) == 0) return "tempo";
  if (memcmp(type, "covr", 4) == 0) return "coverArt";
  if (memcmp(type, "soal", 4) == 0) return "sortAlbum";
  if (memcmp(type, "soar", 4) == 0) return "sortArtist";
  if (memcmp(type, "sonm", 4) == 0) return "sortTitle";
  return NULL;
}

static bool mp4_type_is_printable(const char type[5]) {
  for (int i = 0; i < 4; i++) {
    unsigned char value = (unsigned char)type[i];
    if (value < 0x20 || value > 0x7e) {
      return false;
    }
  }
  return true;
}

static void mp4_fallback_key(const char type[5], char *key, size_t key_size) {
  const char *known = mp4_tag_key(type);
  if (known != NULL) {
    copy_capped_local(key, key_size, known);
  } else if (mp4_type_is_printable(type)) {
    snprintf(key, key_size, "mp4.%c%c%c%c", type[0], type[1], type[2],
             type[3]);
  } else {
    snprintf(key, key_size, "mp4.0x%02x%02x%02x%02x",
             (unsigned char)type[0], (unsigned char)type[1],
             (unsigned char)type[2], (unsigned char)type[3]);
  }
}

static bool mp4_payload_looks_text(const unsigned char *payload, size_t length) {
  if (length == 0) {
    return false;
  }
  size_t printable = 0;
  for (size_t i = 0; i < length; i++) {
    unsigned char value = payload[i];
    if (value == '\0') {
      continue;
    }
    if (value == '\r' || value == '\n' || value == '\t' ||
        (value >= 0x20 && value < 0x7f) || value >= 0x80) {
      printable++;
    } else {
      return false;
    }
  }
  return printable > 0;
}

static void metadata_set_mp4_data(MetadataMap *metadata, const char key[5],
                                  const unsigned char *payload,
                                  size_t payload_size) {
  if (payload_size == 0) {
    return;
  }
  char metadata_key[METADATA_KEY_SIZE];
  mp4_fallback_key(key, metadata_key, sizeof(metadata_key));
  if (payload_size >= 8 && memcmp(payload, "data", 4) != 0) {
    uint32_t data_type = read_u32_be_bytes(payload);
    const unsigned char *data = payload + 8;
    size_t data_size = payload_size - 8;
    if ((data_type == 1 || data_type == 0) &&
        mp4_payload_looks_text(data, data_size)) {
      metadata_set_unique_bytes(metadata, metadata_key, data, data_size);
      return;
    }
    if (data_size == 1) {
      metadata_setf(metadata, metadata_key, "%u", data[0]);
      return;
    }
    if (data_size == 2) {
      metadata_setf(metadata, metadata_key, "%u", read_u16_be_bytes(data));
      return;
    }
    if (data_size == 4) {
      metadata_setf(metadata, metadata_key, "%u", read_u32_be_bytes(data));
      return;
    }
    metadata_setf(metadata, metadata_key, "%zu bytes", data_size);
    return;
  }
  if (mp4_payload_looks_text(payload, payload_size)) {
    metadata_set_unique_bytes(metadata, metadata_key, payload, payload_size);
  } else {
    metadata_setf(metadata, metadata_key, "%zu bytes", payload_size);
  }
}

static void scan_mp4_ilst_item(FILE *file, uint64_t start, uint64_t end,
                               const char item_type[5],
                               MetadataMap *metadata) {
  uint64_t offset = start;
  bool saw_data = false;
  while (offset + 8 <= end) {
    Mp4Box box;
    if (!read_mp4_box(file, offset, end, &box)) {
      return;
    }
    if (is_box_type(box.type, "data") && box.size > box.header_size + 8) {
      uint64_t payload_offset = box.offset + box.header_size;
      uint64_t payload_size = box.size - box.header_size;
      if (payload_size <= 1024 * 1024) {
        unsigned char *payload = (unsigned char *)malloc((size_t)payload_size);
        if (payload != NULL &&
            read_at(file, payload_offset, payload, (size_t)payload_size)) {
          metadata_set_mp4_data(metadata, item_type, payload, (size_t)payload_size);
          saw_data = true;
        }
        free(payload);
      }
    }
    offset += box.size;
  }
  if (!saw_data && end > start && end - start <= 1024 * 1024) {
    size_t payload_size = (size_t)(end - start);
    unsigned char *payload = (unsigned char *)malloc(payload_size);
    if (payload != NULL && read_at(file, start, payload, payload_size)) {
      metadata_set_mp4_data(metadata, item_type, payload, payload_size);
    }
    free(payload);
  }
}

static void scan_mp4_ilst(FILE *file, uint64_t start, uint64_t end,
                          MetadataMap *metadata) {
  uint64_t offset = start;
  while (offset + 8 <= end) {
    Mp4Box box;
    if (!read_mp4_box(file, offset, end, &box)) {
      return;
    }
    scan_mp4_ilst_item(file, box.offset + box.header_size,
                       box.offset + box.size, box.type, metadata);
    offset += box.size;
  }
}

static void scan_mp4_meta(FILE *file, uint64_t start, uint64_t end,
                          MetadataMap *metadata, int depth);

static void scan_mp4_metadata_boxes(FILE *file, uint64_t start, uint64_t end,
                                    MetadataMap *metadata, int depth) {
  if (depth > 8) {
    return;
  }
  uint64_t offset = start;
  while (offset + 8 <= end) {
    Mp4Box box;
    if (!read_mp4_box(file, offset, end, &box)) {
      return;
    }
    uint64_t content = box.offset + box.header_size;
    uint64_t box_end = box.offset + box.size;
    if (is_box_type(box.type, "ilst")) {
      scan_mp4_ilst(file, content, box_end, metadata);
    } else if (is_box_type(box.type, "meta")) {
      scan_mp4_meta(file, content, box_end, metadata, depth + 1);
    } else if (is_box_type(box.type, "udta") || is_box_type(box.type, "moov")) {
      scan_mp4_metadata_boxes(file, content, box_end, metadata, depth + 1);
    }
    offset += box.size;
  }
}

static void scan_mp4_meta(FILE *file, uint64_t start, uint64_t end,
                          MetadataMap *metadata, int depth) {
  if (start + 4 > end) {
    return;
  }
  scan_mp4_metadata_boxes(file, start + 4, end, metadata, depth + 1);
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
    Mp4Box box;
    if (!read_mp4_box(file, offset, (uint64_t)file_size, &box)) {
      break;
    }
    if (is_box_type(box.type, "ftyp")) {
      unsigned char brand_bytes[4];
      if (read_at(file, box.offset + box.header_size, brand_bytes,
                  sizeof(brand_bytes))) {
        memcpy(brand, brand_bytes, 4);
      }
      if (box.size > box.header_size + 8) {
        unsigned char minor_bytes[4];
        if (read_at(file, box.offset + box.header_size + 4, minor_bytes,
                    sizeof(minor_bytes))) {
          metadata_setf_both(&video->metadata, &audio->metadata, "minorVersion",
                             "%u", read_u32_be_bytes(minor_bytes));
        }
        uint64_t compatible_offset = box.offset + box.header_size + 8;
        uint64_t compatible_end = box.offset + box.size;
        int brand_index = 0;
        while (compatible_offset + 4 <= compatible_end) {
          unsigned char compatible_brand[4];
          if (!read_at(file, compatible_offset, compatible_brand,
                       sizeof(compatible_brand))) {
            break;
          }
          char key[METADATA_KEY_SIZE];
          snprintf(key, sizeof(key), "compatibleBrand.%d", brand_index++);
          metadata_set_bytes(&video->metadata, key, compatible_brand, 4);
          metadata_set_bytes(&audio->metadata, key, compatible_brand, 4);
          compatible_offset += 4;
        }
      }
    } else if (is_box_type(box.type, "moov")) {
      moov_start = box.offset + box.header_size;
      moov_end = box.offset + box.size;
    }
    offset += box.size;
  }
  if (moov_start == 0 || brand[0] == '\0') {
    return false;
  }
  metadata_set_bytes(&video->metadata, "majorBrand",
                     (const unsigned char *)brand, 4);
  metadata_set_bytes(&audio->metadata, "majorBrand",
                     (const unsigned char *)brand, 4);
  scan_mp4_metadata_boxes(file, moov_start, moov_end, &video->metadata, 0);
  metadata_copy(&audio->metadata, &video->metadata);
  offset = moov_start;
  while (offset + 8 <= moov_end) {
    Mp4Box box;
    if (!read_mp4_box(file, offset, moov_end, &box)) {
      break;
    }
    if (is_box_type(box.type, "mvhd")) {
      parse_mvhd(file, box.offset + box.header_size, box.size - box.header_size,
                 &timescale, &duration);
    } else if (is_box_type(box.type, "trak")) {
      Mp4Track track =
          scan_trak(file, box.offset + box.header_size, box.offset + box.size);
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
    offset += box.size;
  }
  int64_t duration_micros =
      timescale == 0 ? 0 : (int64_t)((double)duration * 1000000.0 / timescale);
  bool quicktime = memcmp(brand, "qt  ", 4) == 0;
  if (timescale > 0) {
    metadata_setf_both(&video->metadata, &audio->metadata, "timescale", "%u",
                       timescale);
    metadata_setf_both(&video->metadata, &audio->metadata, "durationUnits",
                       "%llu", (unsigned long long)duration);
    metadata_setf_both(&video->metadata, &audio->metadata, "durationMicros",
                       "%lld", (long long)duration_micros);
  }
  if (*is_video) {
    video->duration_micros = duration_micros;
    video->bitrate = duration_micros == 0
                         ? 0
                         : (int)((double)file_size * 8.0 * 1000000.0 /
                                 (double)duration_micros);
    video->format = quicktime ? "quicktime" : "mp4";
    video->container = quicktime ? "QuickTime" : "MP4";
    metadata_set(&video->metadata, "container", video->container);
    metadata_set(&video->metadata, "codec", video->codec);
    metadata_setf(&video->metadata, "width", "%d", video->width);
    metadata_setf(&video->metadata, "height", "%d", video->height);
    if (video->frame_rate > 0.0) {
      metadata_setf(&video->metadata, "frameRate", "%.6f", video->frame_rate);
    }
    if (video->bitrate > 0) {
      metadata_setf(&video->metadata, "bitrate", "%d", video->bitrate);
    }
  }
  if (*is_audio) {
    audio->duration_micros = duration_micros;
    audio->bitrate = duration_micros == 0
                         ? 0
                         : (int)((double)file_size * 8.0 * 1000000.0 /
                                 (double)duration_micros);
    audio->format = quicktime ? "mp4" : "aac";
    audio->container = quicktime ? "QuickTime" : "MP4";
    metadata_set(&audio->metadata, "container", audio->container);
    metadata_set(&audio->metadata, "codec", audio->codec);
    if (audio->sample_rate > 0) {
      metadata_setf(&audio->metadata, "sampleRate", "%d", audio->sample_rate);
    }
    if (audio->channels > 0) {
      metadata_setf(&audio->metadata, "channels", "%d", audio->channels);
    }
    if (audio->bitrate > 0) {
      metadata_setf(&audio->metadata, "bitrate", "%d", audio->bitrate);
    }
  }
  return *is_video || *is_audio;
}

static void append_common(JsonBuilder *builder, const char *kind,
                          const char *path, int64_t size_bytes,
                          const char *format, const MetadataMap *metadata) {
  jb_append(builder, "{\"ok\":true,\"kind\":");
  jb_append_json_string(builder, kind);
  jb_append(builder, ",\"path\":");
  jb_append_json_string(builder, path);
  jb_appendf(builder, ",\"fileSize\":%lld", (long long)size_bytes);
  jb_append(builder, ",\"format\":");
  jb_append_json_string(builder, format);
  jb_append(builder, ",\"metadata\":");
  append_metadata_json(builder, metadata);
}

static char *json_image(const char *path, int64_t size_bytes,
                        const ImageInfo *info) {
  JsonBuilder builder;
  if (!jb_init(&builder)) {
    return NULL;
  }
  append_common(&builder, "image", path, size_bytes, info->format,
                &info->metadata);
  jb_appendf(&builder, ",\"width\":%d,\"height\":%d", info->width,
             info->height);
  jb_append(&builder, ",\"codec\":");
  jb_append_json_string(&builder, info->codec);
  jb_append(&builder, ",\"bitrate\":null,\"exif\":");
  append_metadata_json(&builder, &info->exif);
  jb_append(&builder, "}");
  return builder.data;
}

static char *json_video(const char *path, int64_t size_bytes,
                        const VideoInfo *info) {
  JsonBuilder builder;
  if (!jb_init(&builder)) {
    return NULL;
  }
  append_common(&builder, "video", path, size_bytes, info->format,
                &info->metadata);
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
                &info->metadata);
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

static void image_info_free(ImageInfo *info) {
  metadata_free(&info->metadata);
  metadata_free(&info->exif);
}

static void video_info_free(VideoInfo *info) {
  metadata_free(&info->metadata);
}

static void audio_info_free(AudioInfo *info) {
  metadata_free(&info->metadata);
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
    char *result = json_image(path, size_bytes, &image);
    image_info_free(&image);
    return result;
  }
  image_info_free(&image);

  AudioInfo audio;
  memset(&audio, 0, sizeof(audio));
  if (parse_wav(file, size_bytes, &audio)) {
    fclose(file);
    char *result = json_audio(path, size_bytes, &audio);
    audio_info_free(&audio);
    return result;
  }

  VideoInfo video;
  memset(&video, 0, sizeof(video));
  bool is_audio = false;
  bool is_video = false;
  if (parse_mp4(file, size_bytes, &video, &audio, &is_audio, &is_video)) {
    fclose(file);
    char *result;
    if (is_video) {
      result = json_video(path, size_bytes, &video);
    } else {
      result = json_audio(path, size_bytes, &audio);
    }
    video_info_free(&video);
    audio_info_free(&audio);
    return result;
  }
  video_info_free(&video);
  audio_info_free(&audio);

  fclose(file);
  return json_error(path, "unsupported",
                    "File is not a supported image, video, or audio file.");
}

void fmt_free_string(char *value) { free(value); }
