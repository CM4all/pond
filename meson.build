project('CM4all Pond', 'cpp', version: '0.33',
  meson_version: '>= 0.56',
  default_options: [
    'cpp_std=c++2a',
    'warning_level=3',

    'fmt:cpp_std=c++2a',
  ],
)

debug = get_option('b_ndebug') != 'true'

compiler = meson.get_compiler('cpp')

conf = configuration_data()

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
  '-Wvla',
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

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wcomma-subscript',
  '-Wextra-semi',
  '-Wmismatched-tags',
  '-Woverloaded-virtual',
  '-Wsign-promo',
  '-Wvolatile',
  '-Wvirtual-inheritance',

  '-Wno-missing-field-initializers',

  # a vtable without a dtor is just fine
  '-Wno-non-virtual-dtor',

  # clang specific warning options:
  '-Wcomma',
  '-Wheader-hygiene',
  '-Winconsistent-missing-destructor-override',
]

add_global_arguments(common_flags, language: 'c')
add_global_arguments(common_flags, language: 'cpp')

add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')

libsystemd = dependency('libsystemd', required: get_option('systemd'))
libgeoip = dependency('geoip', required: get_option('geoip'))

inc = include_directories('src', 'libcommon/src')

libcommon_require_avahi = get_option('zeroconf')

subdir('libcommon/src/util')
subdir('libcommon/src/lib/fmt')
subdir('libcommon/src/time')
subdir('libcommon/src/http')
subdir('libcommon/src/io')
subdir('libcommon/src/io/config')
subdir('libcommon/src/system')
subdir('libcommon/src/event')
subdir('libcommon/src/net')
subdir('libcommon/src/net/log')
subdir('libcommon/src/event/net')
subdir('libcommon/src/event/net/log')
subdir('libcommon/src/lib/avahi')
subdir('libcommon/src/lib/zlib')

conf.set('HAVE_LIBSYSTEMD', libsystemd.found())
conf.set('HAVE_LIBGEOIP', libgeoip.found())
conf.set('HAVE_AVAHI', avahi_dep.found())
configure_file(output: 'config.h', configuration: conf)

daemon_sources = []

if avahi_dep.found()
  daemon_sources += 'src/AutoClone.cxx'
endif

executable('cm4all-pond',
  'src/Main.cxx',
  'src/CommandLine.cxx',
  'src/Instance.cxx',
  'src/Receiver.cxx',
  'src/Listener.cxx',
  'src/Connection.cxx',
  'src/Clone.cxx',
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
  daemon_sources,
  'libcommon/src/pg/Interval.cxx',
  include_directories: inc,
  dependencies: [
    libsystemd,
    fmt_dep,
    event_net_dep,
    event_net_log_dep,
    system_dep,
    io_dep,
    io_config_dep,
    http_dep,
    avahi_dep,
  ],
  install: true,
  install_dir: 'sbin')

client_sources = []

if avahi_dep.found()
  client_sources += [
    'src/client/SimpleAddressResolver.cxx',
    'src/client/CachedAddressResolver.cxx',
  ]
endif

executable('cm4all-pond-client',
  'src/client/Main.cxx',
  'src/client/ResultWriter.cxx',
  'src/client/FormatJson.cxx',
  'src/client/VisitorTracker.cxx',
  'src/client/PerSitePath.cxx',
  'src/client/Client.cxx',
  'src/client/Send.cxx',
  'src/client/Open.cxx',
  client_sources,
  include_directories: inc,
  dependencies: [
    libgeoip,
    fmt_dep,
    net_dep,
    net_log_dep,
    event_net_dep,
    http_dep,
    avahi_dep,
    zlib_dep,
  ],
  install: true)

subdir('test')
subdir('libcommon/test/util')
subdir('libcommon/test/http')
subdir('libcommon/test/io/config')
subdir('libcommon/test/net')
subdir('libcommon/test/time')
subdir('doc')
