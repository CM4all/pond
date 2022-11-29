gtest_compile_args = [
  '-Wno-undef',
]

if compiler.get_id() == 'gcc'
  gtest_compile_args += [
    '-Wno-suggest-attribute=format',
    '-Wno-suggest-attribute=noreturn',
    '-Wno-missing-declarations',
  ]
endif

if compiler.get_id() == 'clang' and compiler.version().version_compare('>=9')
  gtest_compile_args += [
    # work around clang warning caused by GTest's wrong "-lpthread"
    # compiler flag
    '-Wno-unused-command-line-argument',
  ]
endif

gtest_main_dep = dependency('gtest', main: true, fallback: ['gtest', 'gtest_main_dep'])
gtest = declare_dependency(
  dependencies: [gtest_main_dep],
  compile_args: gtest_compile_args,
)

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
