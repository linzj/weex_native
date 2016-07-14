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
      'conditions': [
        [ 'want_separate_host_toolset==1', {
          'toolsets': [ 'target', ],
        }],
        ['OS=="win" and v8_enable_i18n_support==1', {
          'dependencies': [
            '<(icu_gyp_path):icudata',
          ],
        }],
      ],
    },
  ],
}
