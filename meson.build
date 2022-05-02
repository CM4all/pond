project('CM4all Pond', ['c', 'cpp'], version: '0.22',
  meson_version: '>= 0.47',
  default_options: [
    'c_std=c11',
    'cpp_std=c++2a',
    'warning_level=3',
  ],
)

debug = get_option('b_ndebug') != 'true'

compiler = meson.get_compiler('cpp')
c_compiler = meson.get_compiler('c')

common_flags = [
  '-D_GNU_SOURCE',
]

test_common_flags = [
  '-Wcast-align',
  '-Wcast-qual',
  '-Wdouble-promotion',
  '-Wfloat-equal',
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wmissing-format-attribute',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wundef',
  '-Wunused',
  '-Wwrite-strings',

  # clang specific warning options:
  '-Wunreachable-code-aggressive',
  '-Wused-but-marked-unused',
]

test_global_common_flags = [
  '-fvisibility=hidden',
]

add_global_link_arguments(
  compiler.get_supported_link_arguments(
    # make relocations read-only (hardening)
    '-Wl,-z,relro',

    # no lazy binding, please - not worth it for a daemon
    '-Wl,-z,now',
  ),
  language: 'cpp'
)

if not debug
  test_global_common_flags += [
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

test_global_cxxflags = test_global_common_flags + [
]

test_global_cflags = test_global_common_flags + [
]

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wno-missing-field-initializers',

  # a vtable without a dtor is just fine
  '-Wno-non-virtual-dtor',

  # the only warnings we got from this are from formatted error
  # messages, and their truncation is harmless
  '-Wno-format-truncation',

  # clang specific warning options:
  '-Wcomma',
  '-Wheader-hygiene',
  '-Winconsistent-missing-destructor-override',
]

test_cflags = test_common_flags + [
  '-Waggregate-return',
  '-Wbad-function-cast',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wstrict-prototypes',
]

add_global_arguments(common_flags, language: 'c')
add_global_arguments(common_flags, language: 'cpp')

add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_global_arguments(c_compiler.get_supported_arguments(test_global_cflags), language: 'c')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')
add_project_arguments(c_compiler.get_supported_arguments(test_cflags), language: 'c')

libsystemd = dependency('libsystemd')
libgeoip = dependency('geoip')

inc = include_directories('src', 'libcommon/src')

libcommon_enable_libcap = false
subdir('libcommon/src/util')
subdir('libcommon/src/time')
subdir('libcommon/src/http')
subdir('libcommon/src/io')
subdir('libcommon/src/system')
subdir('libcommon/src/event')
subdir('libcommon/src/net')
subdir('libcommon/src/event/net')
subdir('libcommon/src/lib/avahi')
subdir('libcommon/src/lib/zlib')

executable('cm4all-pond',
  'src/Main.cxx',
  'src/CommandLine.cxx',
  'src/Instance.cxx',
  'src/Receiver.cxx',
  'src/Listener.cxx',
  'src/Connection.cxx',
  'src/Clone.cxx',
  'src/AutoClone.cxx',
  'src/Config.cxx',
  'src/Database.cxx',
  'src/RList.cxx',
  'src/FullRecordList.cxx',
  'src/AnyList.cxx',
  'src/RSkipDeque.cxx',
  'src/Record.cxx',
  'src/Filter.cxx',
  'src/LightCursor.cxx',
  'src/Cursor.cxx',
  'src/Selection.cxx',
  'src/SendQueue.cxx',
  'src/client/Async.cxx',
  'src/client/Send.cxx',
  'src/system/SetupProcess.cxx',
  'libcommon/src/pg/Interval.cxx',
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
  'src/client/ResultWriter.cxx',
  'src/client/VisitorTracker.cxx',
  'src/client/PerSitePath.cxx',
  'src/client/Client.cxx',
  'src/client/Send.cxx',
  'src/client/Open.cxx',
  include_directories: inc,
  dependencies: [
    libgeoip,
    net_dep,
    event_net_dep,
    http_dep,
    avahi_dep,
    zlib_dep,
  ],
  install: true)

subdir('test')
subdir('libcommon/test/util')
subdir('libcommon/test/http')
subdir('libcommon/test/io')
subdir('libcommon/test/net')
subdir('libcommon/test/time')
subdir('doc')
