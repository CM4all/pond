libpond_client_common = static_library(
  'pond_client_common',
  'Send.cxx',
  include_directories: inc,
  dependencies: [
    io_dep,
    net_dep,
    net_log_dep,
    system_dep,
    util_dep,
  ],
)

libpond_client_common_dep = declare_dependency(
  link_with: libpond_client_common,
  dependencies: [
    net_dep,
    net_log_dep,
    util_dep,
  ],
)

libpond_client_sync = static_library(
  'pond_client_sync',
  'Client.cxx',
  'Open.cxx',
  include_directories: inc,
  dependencies: [
    libpond_client_common_dep,
    avahi_dep,
    event_dep,
    event_net_dep,
    net_dep,
    util_dep,
  ],
)

libpond_client_sync_dep = declare_dependency(
  link_with: libpond_client_sync,
  dependencies: [
    libpond_client_common_dep,
    net_dep,
    util_dep,
  ],
)

libpond_client_async = static_library(
  'pond_client_async',
  'Async.cxx',
  include_directories: inc,
  dependencies: [
    libpond_client_common_dep,
    event_dep,
    net_dep,
    util_dep,
  ],
)

libpond_client_async_dep = declare_dependency(
  link_with: libpond_client_async,
  dependencies: [
    libpond_client_common_dep,
    event_dep,
    net_dep,
    util_dep,
  ],
)
