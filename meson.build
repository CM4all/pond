project('CM4all Pond', ['c', 'cpp'], version: '0.9',
  meson_version: '>= 0.47',
  default_options: [
    'c_std=c99',
    'cpp_std=c++14'
  ],
)

# TODO: use get_option('b_ndebug'), but that doesn't work with Meson 0.37.1
if get_option('buildtype') == 'debug'
  debug = true
else
  debug = false
endif

compiler = meson.get_compiler('cpp')
c_compiler = meson.get_compiler('c')

common_flags = [
  '-D_GNU_SOURCE',
  '-DPACKAGE="' + meson.project_name() + '"',
  '-DVERSION="' + meson.project_version() + '"',
  '-Wall',
  '-Wextra',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wunused',
  '-Wundef',
]

if debug
  common_flags += ['-DPOISON']
endif

test_common_flags = [
  '-fvisibility=hidden',
]

if not debug
  test_common_flags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]

  add_global_link_arguments(
    compiler.get_supported_link_arguments(
      '-Wl,--gc-sections',
      '-Wl,--icf=all',
    ),
    language: 'cpp'
  )
endif

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wno-missing-field-initializers',
  '-Wno-non-virtual-dtor',
]

test_cflags = test_common_flags + [
  '-Wmissing-prototypes', '-Wstrict-prototypes',
  '-Wbad-function-cast',
  '-Waggregate-return',
  '-Wnested-externs',
  '-pedantic',
]

add_global_arguments(common_flags, language: 'c')
add_global_arguments(common_flags, language: 'cpp')

add_global_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')
add_global_arguments(c_compiler.get_supported_arguments(test_cflags), language: 'c')

boost = dependency('boost', modules: ['system', 'filesystem'], version: '>= 1.54')
libsystemd = dependency('libsystemd')

gtest_compile_args = [
  '-Wno-undef',
]

if compiler.get_id() == 'gcc'
  gtest_compile_args += [
    '-Wno-suggest-attribute=format',
    '-Wno-suggest-attribute=noreturn',
    '-Wno-missing-declarations',

    # needed on Jessie for gtest's IsNullLiteralHelper
    '-Wno-conversion-null',
  ]
endif

gtest = declare_dependency(
  dependencies: [dependency('gtest', main: true)],
  compile_args: gtest_compile_args,
)

inc = include_directories('src', 'libcommon/src')

subdir('libcommon/src/util')
subdir('libcommon/src/time')
subdir('libcommon/src/http')
subdir('libcommon/src/io')
subdir('libcommon/src/system')
subdir('libcommon/src/event')
subdir('libcommon/src/net')
subdir('libcommon/src/event/net')
subdir('libcommon/src/avahi')

executable('cm4all-pond',
  'src/Main.cxx',
  'src/Instance.cxx',
  'src/Receiver.cxx',
  'src/Listener.cxx',
  'src/Connection.cxx',
  'src/Clone.cxx',
  'src/Config.cxx',
  'src/Database.cxx',
  'src/RList.cxx',
  'src/AnyList.cxx',
  'src/RSkipDeque.cxx',
  'src/Record.cxx',
  'src/Filter.cxx',
  'src/LightCursor.cxx',
  'src/Cursor.cxx',
  'src/Selection.cxx',
  'src/client/Client.cxx',
  'src/system/SetupProcess.cxx',
  include_directories: inc,
  dependencies: [
    libsystemd,
    event_net_dep,
    system_dep,
    io_dep,
    http_dep,
    avahi_dep,
  ],
  install: true,
  install_dir: 'sbin')

executable('cm4all-pond-client',
  'src/client/Main.cxx',
  'src/client/Client.cxx',
  'src/client/Open.cxx',
  include_directories: inc,
  dependencies: [
    net_dep,
    event_net_dep,
    http_dep,
    avahi_dep,
  ],
  install: true)

subdir('libcommon/test/util')
subdir('libcommon/test/http')
subdir('libcommon/test/io')
subdir('libcommon/test/net')
subdir('doc')
