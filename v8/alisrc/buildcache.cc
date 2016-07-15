#include <stdio.h>
#include <stdlib.h>
#include <v8.h>
#include "include/libplatform/libplatform.h"
#include <memory>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace v8;

static void SetFlagsFromString(const char* flags) {
  v8::V8::SetFlagsFromString(flags, static_cast<int>(strlen(flags)));
}

static ScriptCompiler::CachedData* CompileForCachedData(
    Local<String> source,
    Local<Value> name,
    ScriptCompiler::CompileOptions compile_options) {
  if (source.IsEmpty()) {
    return nullptr;
  }
  int source_length = source->Length();
  uint16_t* source_buffer = new uint16_t[source_length];
  source->Write(source_buffer, 0, source_length);
  int name_length = 0;
  uint16_t* name_buffer = NULL;
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  if (name->IsString()) {
    Local<String> name_string = Local<String>::Cast(name);
    name_length = name_string->Length();
    name_buffer = new uint16_t[name_length];
    name_string->Write(name_buffer, 0, name_length);
  }
  Isolate* temp_isolate = Isolate::New(create_params);
  ScriptCompiler::CachedData* result = NULL;
  {
    Isolate::Scope isolate_scope(temp_isolate);
    HandleScope handle_scope(temp_isolate);
    Context::Scope context_scope(Context::New(temp_isolate));
    Local<String> source_copy =
        v8::String::NewFromTwoByte(temp_isolate, source_buffer,
                                   v8::NewStringType::kNormal, source_length)
            .ToLocalChecked();
    Local<Value> name_copy;
    if (name_buffer) {
      name_copy =
          v8::String::NewFromTwoByte(temp_isolate, name_buffer,
                                     v8::NewStringType::kNormal, name_length)
              .ToLocalChecked();
    } else {
      name_copy = v8::Undefined(temp_isolate);
    }
    ScriptCompiler::Source script_source(source_copy, ScriptOrigin(name_copy));
    if (!ScriptCompiler::CompileUnboundScript(temp_isolate, &script_source,
                                              compile_options)
             .IsEmpty() &&
        script_source.GetCachedData()) {
      int length = script_source.GetCachedData()->length;
      uint8_t* cache = new uint8_t[length];
      memcpy(cache, script_source.GetCachedData()->data, length);
      result = new ScriptCompiler::CachedData(
          cache, length, ScriptCompiler::CachedData::BufferOwned);
    }
  }
  temp_isolate->Dispose();
  delete[] source_buffer;
  delete[] name_buffer;
  return result;
}

static void StoreCacheData(const ScriptCompiler::CachedData* cache,
                           const char* filename) {
  FILE* f;
  f = fopen(filename, "wb");
  if (!f)
    return;
  if (cache->length != fwrite(cache->data, 1, cache->length, f)) {
    fprintf(stderr, "fails to write fully data to file: %s.\n", filename);
  }
  fclose(f);
}

static FILE* FOpen(const char* path, const char* mode) {
#if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64))
  FILE* result;
  if (fopen_s(&result, path, mode) == 0) {
    return result;
  } else {
    return NULL;
  }
#else
  FILE* file = fopen(path, mode);
  if (file == NULL)
    return NULL;
  struct stat file_stat;
  if (fstat(fileno(file), &file_stat) != 0)
    return NULL;
  bool is_regular_file = ((file_stat.st_mode & S_IFREG) != 0);
  if (is_regular_file)
    return file;
  fclose(file);
  return NULL;
#endif
}

static char* ReadChars(Isolate* isolate, const char* name, int* size_out) {
  FILE* file = FOpen(name, "rb");
  if (file == NULL) {
    fprintf(stderr, "fails to open file: %s.\n", name);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (size_t i = 0; i < size;) {
    i += fread(&chars[i], 1, size - i, file);
    if (ferror(file)) {
      fclose(file);
      delete[] chars;
      return nullptr;
    }
  }
  fclose(file);
  *size_out = static_cast<int>(size);
  return chars;
}

static Local<String> ReadFile(Isolate* isolate, const char* name) {
  int size = 0;
  char* chars = ReadChars(isolate, name, &size);
  if (chars == NULL)
    return Local<String>();
  Local<String> result =
      String::NewFromUtf8(isolate, chars, NewStringType::kNormal, size)
          .ToLocalChecked();
  delete[] chars;
  return result;
}

int main(int argc, const char** argv) {
  if (argc <= 1) {
    fprintf(stderr, "need a file name.\n");
  }
  V8::InitializeICUDefaultLocation(argv[0]);
  // V8::InitializeExternalStartupData(argv[0]);
  Platform* platform = platform::CreateDefaultPlatform();
  V8::InitializePlatform(platform);
  V8::Initialize();
  SetFlagsFromString("--nolazy");

  // Create a new Isolate and make it the current one.
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();

  Isolate* isolate = Isolate::New(create_params);
  {
    Isolate::Scope isolate_scope(isolate);

    // Create a stack-allocated handle scope.
    HandleScope handle_scope(isolate);
    std::unique_ptr<ScriptCompiler::CachedData> cache(
        CompileForCachedData(ReadFile(isolate, argv[1]), Undefined(isolate),
                             ScriptCompiler::kProduceCodeCache));
    if (cache.get() != nullptr) {
      fprintf(stderr, "cache length: %d\n", cache->length);
      StoreCacheData(cache.get(), "cache.data");
    } else {
      fprintf(stderr, "fails to create cache.\n");
    }
  }
  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  V8::Dispose();
  V8::ShutdownPlatform();
  delete platform;
  delete create_params.array_buffer_allocator;
  return 0;
}
