# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

defines = {}
for entry in os.environ['ION_DEFINITION'].split():
    key, value = entry.split('=')
    defines[key] = value

setattr(sys.modules[__name__], 'defines', defines)
setattr(sys.modules[__name__], 'non_global_defines', {})

substs = dict()
substs['CXX'] = 'arm-linux-androideabi-g++'
substs['CC'] = 'arm-linux-androideabi-gcc'
substs['PREPROCESS_OPTION'] = '-E -o '
substs['MOZ_DEBUG_DEFINES'] = []

for var in os.environ:
    if var != 'SHELL' and var in substs:
        substs[var] = os.environ[var]
