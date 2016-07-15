{
  'includes': ['../gypfiles/toolchain.gypi', '../gypfiles/features.gypi'],
  'targets': [
    {
      'target_name': 'buildcache',
      'type': 'executable',
      'dependencies': [
        'v8.gyp:v8',
        'v8.gyp:v8_libplatform',
      ],
      # Generated source files need this explicitly:
      'include_dirs+': [
        '..',
      ],
      'sources': [
        'buildcache.cc',
      ],
    },
  ],
}
