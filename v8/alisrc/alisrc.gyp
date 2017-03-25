{
  'includes': ['../gypfiles/toolchain.gypi', '../gypfiles/features.gypi'],
  'targets': [
    {
      'target_name': 'buildcache',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/src/v8.gyp:v8',
        '<(DEPTH)/src/v8.gyp:v8_libplatform',
      ],
      # Generated source files need this explicitly:
      'include_dirs+': [
        '..',
      ],
      'sources': [
        'buildcache.cc',
      ],
    },
    {
        'target_name': 'weexv8',
        'type': 'shared_library',
        'dependencies': [
          '<(DEPTH)/src/v8.gyp:v8',
          '<(DEPTH)/src/v8.gyp:v8_libplatform',
        ],
        # Generated source files need this explicitly:
        'include_dirs+': [
          '..',
        ],
        'sources': [
          'weexcore.cpp',
          'LogUtils.h',
          'CachedMetaData.cpp',
          'Semaphore.cpp',
          'StringHashMap.cpp',
          'Trace.cpp',
          'V8DefaultPlatform.cpp',
          'V8ScriptRunner.cpp',
          'V8TaskQueue.cpp',
          'V8WorkerThread.cpp',
        ],
        'ldflags':[
            '-Wl,--version-script=<(DEPTH)/alisrc/weexcore_version_script.txt',
            '-Wl,--gc-sections',
            '-Wl,--build-id=sha1',
        ],
        'ldflags!':[
            '-pie',
        ],
        'libraries': [
            '-llog',
        ],
    },
  ],
}
