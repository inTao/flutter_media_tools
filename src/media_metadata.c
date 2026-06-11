#include "media_metadata.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int metadata_index_of(const MetadataMap *map, const char *key) {
  for (size_t i = 0; i < map->count; i++) {
    if (strcmp(map->entries[i].key, key) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static bool metadata_reserve(MetadataMap *map, size_t extra) {
  if (map->count + extra <= map->capacity) {
    return true;
  }
  size_t next_capacity = map->capacity == 0 ? 16 : map->capacity * 2;
  while (map->count + extra > next_capacity) {
    next_capacity *= 2;
  }
  MetadataEntry *next =
      (MetadataEntry *)realloc(map->entries, next_capacity * sizeof(MetadataEntry));
  if (next == NULL) {
    return false;
  }
  map->entries = next;
  map->capacity = next_capacity;
  return true;
}

static void copy_capped(char *destination, size_t destination_size,
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

static void copy_sanitized_bytes(char *destination, size_t destination_size,
                                 const unsigned char *source, size_t length) {
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

void metadata_free(MetadataMap *map) {
  free(map->entries);
  map->entries = NULL;
  map->count = 0;
  map->capacity = 0;
}

bool metadata_set(MetadataMap *map, const char *key, const char *value) {
  if (key == NULL || key[0] == '\0' || value == NULL || value[0] == '\0') {
    return true;
  }
  int existing = metadata_index_of(map, key);
  if (existing >= 0) {
    copy_capped(map->entries[existing].value, METADATA_VALUE_SIZE, value);
    return true;
  }
  if (!metadata_reserve(map, 1)) {
    return false;
  }
  copy_capped(map->entries[map->count].key, METADATA_KEY_SIZE, key);
  copy_capped(map->entries[map->count].value, METADATA_VALUE_SIZE, value);
  map->count++;
  return true;
}

bool metadata_set_unique(MetadataMap *map, const char *key, const char *value) {
  if (metadata_index_of(map, key) < 0) {
    return metadata_set(map, key, value);
  }
  for (int suffix = 2; suffix < 1000; suffix++) {
    char next_key[METADATA_KEY_SIZE];
    snprintf(next_key, sizeof(next_key), "%s.%d", key, suffix);
    if (metadata_index_of(map, next_key) < 0) {
      return metadata_set(map, next_key, value);
    }
  }
  return true;
}

bool metadata_setf(MetadataMap *map, const char *key, const char *format, ...) {
  char value[METADATA_VALUE_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(value, sizeof(value), format, args);
  va_end(args);
  return metadata_set(map, key, value);
}

bool metadata_set_bytes(MetadataMap *map, const char *key,
                        const unsigned char *bytes, size_t length) {
  char value[METADATA_VALUE_SIZE];
  copy_sanitized_bytes(value, sizeof(value), bytes, length);
  return metadata_set(map, key, value);
}

bool metadata_set_unique_bytes(MetadataMap *map, const char *key,
                               const unsigned char *bytes, size_t length) {
  char value[METADATA_VALUE_SIZE];
  copy_sanitized_bytes(value, sizeof(value), bytes, length);
  return metadata_set_unique(map, key, value);
}

bool metadata_copy(MetadataMap *target, const MetadataMap *source) {
  for (size_t i = 0; i < source->count; i++) {
    if (!metadata_set(target, source->entries[i].key, source->entries[i].value)) {
      return false;
    }
  }
  return true;
}

void metadata_setf_both(MetadataMap *first, MetadataMap *second,
                        const char *key, const char *format, ...) {
  char value[METADATA_VALUE_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(value, sizeof(value), format, args);
  va_end(args);
  metadata_set(first, key, value);
  metadata_set(second, key, value);
}
