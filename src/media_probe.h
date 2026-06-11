#ifndef FLUTTER_MEDIA_TOOLS_MEDIA_PROBE_H_
#define FLUTTER_MEDIA_TOOLS_MEDIA_PROBE_H_

#if _WIN32
#define FFI_PLUGIN_EXPORT __declspec(dllexport)
#else
#define FFI_PLUGIN_EXPORT
#endif

FFI_PLUGIN_EXPORT char *fmt_probe_media_file(const char *path);
FFI_PLUGIN_EXPORT void fmt_free_string(char *value);

#endif  // FLUTTER_MEDIA_TOOLS_MEDIA_PROBE_H_
