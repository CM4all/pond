test(
  'TestDatabase',
  executable(
    'TestDatabase',
    'TestDatabase.cxx',
    '../src/Database.cxx',
    '../src/RList.cxx',
    '../src/AnyList.cxx',
    '../src/RSkipDeque.cxx',
    '../src/Record.cxx',
    '../src/Filter.cxx',
    '../src/LightCursor.cxx',
    '../src/Cursor.cxx',
    '../src/Selection.cxx',
    include_directories: inc,
    dependencies: [
      gtest,
      system_dep,
      net_dep,
      http_dep,
    ],
  ),
)
