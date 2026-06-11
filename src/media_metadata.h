#ifndef FLUTTER_MEDIA_TOOLS_MEDIA_METADATA_H_
#define FLUTTER_MEDIA_TOOLS_MEDIA_METADATA_H_

#include <stdbool.h>
#include <stddef.h>

#define METADATA_KEY_SIZE 96
#define METADATA_VALUE_SIZE 512

typedef struct {
  char key[METADATA_KEY_SIZE];
  char value[METADATA_VALUE_SIZE];
} MetadataEntry;

typedef struct {
  MetadataEntry *entries;
  size_t count;
  size_t capacity;
} MetadataMap;

void metadata_free(MetadataMap *map);
bool metadata_set(MetadataMap *map, const char *key, const char *value);
bool metadata_set_unique(MetadataMap *map, const char *key, const char *value);
bool metadata_setf(MetadataMap *map, const char *key, const char *format, ...);
bool metadata_set_bytes(MetadataMap *map, const char *key,
                        const unsigned char *bytes, size_t length);
bool metadata_set_unique_bytes(MetadataMap *map, const char *key,
                               const unsigned char *bytes, size_t length);
bool metadata_copy(MetadataMap *target, const MetadataMap *source);
void metadata_setf_both(MetadataMap *first, MetadataMap *second,
                        const char *key, const char *format, ...);

#endif  // FLUTTER_MEDIA_TOOLS_MEDIA_METADATA_H_
