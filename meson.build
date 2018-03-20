project('CM4all Pond', ['c', 'cpp'], version: '0.6',
  default_options: [
    'c_std=c99',
    'cpp_std=c++14'
  ],
)

compiler = meson.get_compiler('cpp')

# TODO: use get_option('b_ndebug'), but that doesn't work with Meson 0.37.1
if get_option('buildtype') == 'debug'
  debug = true
else
  debug = false
endif

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

add_global_arguments(common_flags,
  '-Wmissing-prototypes', '-Wstrict-prototypes',
  '-Wbad-function-cast',
  '-Waggregate-return',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wnested-externs',
  '-pedantic',
  language: 'c')

add_global_arguments(common_flags,
  '-Wno-missing-field-initializers',
  '-Wno-non-virtual-dtor',
  language: 'cpp')

boost = dependency('boost', modules: ['system', 'filesystem'], version: '>= 1.54')
libevent = dependency('libevent', version: '>= 2.0.19')
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

util = static_library('util',
  'libcommon/src/util/AllocatedString.cxx',
  'libcommon/src/util/Exception.cxx',
  'libcommon/src/util/LeakDetector.cxx',
  'libcommon/src/util/PrintException.cxx',
  'libcommon/src/util/StringBuilder.cxx',
  'libcommon/src/util/StringParser.cxx',
  'libcommon/src/util/StringUtil.cxx',
  'libcommon/src/util/StringView.cxx',
  'libcommon/src/util/djbhash.c',
  include_directories: inc,
  dependencies: [
  ])
util_dep = declare_dependency(link_with: util)

time = static_library('time',
  'libcommon/src/time/gmtime.c',
  'libcommon/src/time/Convert.cxx',
  'libcommon/src/time/ISO8601.cxx',
  include_directories: inc,
  dependencies: [
  ])
time_dep = declare_dependency(link_with: time)

http = static_library('http',
  'libcommon/src/http/Method.c',
  'libcommon/src/http/Status.c',
  'libcommon/src/http/HeaderName.cxx',
  'libcommon/src/http/List.cxx',
  'libcommon/src/http/Date.cxx',
  'libcommon/src/http/Range.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
    time_dep,
  ])
http_dep = declare_dependency(link_with: http)

system = static_library('system',
  'src/system/SetupProcess.cxx',
  'libcommon/src/system/ProcessName.cxx',
  'libcommon/src/system/BindMount.cxx',
  'libcommon/src/system/LargeAllocation.cxx',
  include_directories: inc,
  dependencies: [
  ])
system_dep = declare_dependency(link_with: system)

io = static_library('io',
  'libcommon/src/io/FileDescriptor.cxx',
  'libcommon/src/io/WriteFile.cxx',
  'libcommon/src/io/Logger.cxx',
  'libcommon/src/io/LineParser.cxx',
  'libcommon/src/io/FileLineParser.cxx',
  'libcommon/src/io/ConfigParser.cxx',
  include_directories: inc,
  dependencies: [
    boost,
    util_dep,
  ])
io_dep = declare_dependency(link_with: io)

event = static_library('event',
  'libcommon/src/event/Loop.cxx',
  'libcommon/src/event/ShutdownListener.cxx',
  'libcommon/src/event/CleanupTimer.cxx',
  'libcommon/src/event/DeferEvent.cxx',
  'libcommon/src/event/SignalEvent.cxx',
  'libcommon/src/event/PipeLineReader.cxx',
  include_directories: inc,
  dependencies: [
    libevent,
    util_dep,
  ])
event_dep = declare_dependency(link_with: event)

net = static_library('net',
  'libcommon/src/net/SocketAddress.cxx',
  'libcommon/src/net/StaticSocketAddress.cxx',
  'libcommon/src/net/AllocatedSocketAddress.cxx',
  'libcommon/src/net/MaskedSocketAddress.cxx',
  'libcommon/src/net/IPv4Address.cxx',
  'libcommon/src/net/IPv6Address.cxx',
  'libcommon/src/net/ToString.cxx',
  'libcommon/src/net/HostParser.cxx',
  'libcommon/src/net/AddressInfo.cxx',
  'libcommon/src/net/Resolver.cxx',
  'libcommon/src/net/Parser.cxx',
  'libcommon/src/net/RBindSocket.cxx',
  'libcommon/src/net/RConnectSocket.cxx',
  'libcommon/src/net/Interface.cxx',
  'libcommon/src/net/SocketDescriptor.cxx',
  'libcommon/src/net/UniqueSocketDescriptor.cxx',
  'libcommon/src/net/Buffered.cxx',
  'libcommon/src/net/SocketConfig.cxx',
  'libcommon/src/net/MultiReceiveMessage.cxx',
  'libcommon/src/net/log/String.cxx',
  'libcommon/src/net/log/Parser.cxx',
  'libcommon/src/net/log/OneLine.cxx',
  include_directories: inc,
  dependencies: [
    event_dep,
    system_dep,
    io_dep,
  ])
net_dep = declare_dependency(link_with: net)

event_net = static_library('event_net',
  'libcommon/src/event/net/ServerSocket.cxx',
  'libcommon/src/event/net/MultiUdpListener.cxx',
  'libcommon/src/event/net/SocketWrapper.cxx',
  'libcommon/src/event/net/BufferedSocket.cxx',
  include_directories: inc,
  dependencies: [
    libevent,
    event_dep,
    net_dep,
    util_dep,
  ])
event_net_dep = declare_dependency(link_with: event_net)

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
  include_directories: inc,
  dependencies: [
    libsystemd,
    event_net_dep,
    system_dep,
    io_dep,
    http_dep,
  ],
  install: true,
  install_dir: 'sbin')

executable('cm4all-pond-client',
  'src/client/Main.cxx',
  'src/client/Client.cxx',
  include_directories: inc,
  dependencies: [
    net_dep,
    http_dep,
  ],
  install: true)

subdir('libcommon/test/util')
subdir('libcommon/test/http')
subdir('libcommon/test/io')
subdir('libcommon/test/net')
subdir('doc')
