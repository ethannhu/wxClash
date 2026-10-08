project(
  'WxClash',
  'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++17',
    'b_vscrt=mt',
  ],
)

wx_dep = dependency(
  'wxwidgets',
  modules: ['core', 'base', 'net'],
  required: false,
)
if not wx_dep.found()
  cmake = import('cmake')
  wx_subproject = cmake.subproject(
    'wxwidgets',
    options: [
      'wxBUILD_SAMPLES=OFF',
      'wxBUILD_TESTS=OFF',
      'wxBUILD_DEMOS=OFF',
      'wxBUILD_SHARED=OFF',
      'wxBUILD_MONOLITHIC=OFF',
    ],
  )
  wx_dep = declare_dependency(dependencies: [
    wx_subproject.dependency('wx::core'),
    wx_subproject.dependency('wx::base'),
    wx_subproject.dependency('wx::net'),
  ])
endif

yaml_cpp_dep = dependency('yaml-cpp', required: false)
if not yaml_cpp_dep.found()
  cmake = import('cmake')
  yaml_subproject = cmake.subproject(
    'yaml-cpp',
    options: [
      'YAML_BUILD_SHARED_LIBS=OFF',
      'YAML_CPP_BUILD_CONTRIB=OFF',
      'YAML_CPP_BUILD_TOOLS=OFF',
      'YAML_CPP_BUILD_TESTS=OFF',
      'YAML_CPP_INSTALL=OFF',
    ],
  )
  yaml_cpp_dep = yaml_subproject.dependency('yaml-cpp')
endif
nlohmann_json_dep = dependency(
  'nlohmann_json',
  required: true,
  fallback: ['nlohmann_json', 'nlohmann_json_dep'],
)

cpp_args = []
if host_machine.system() == 'windows'
  # Declare Windows 7 as the minimum target system.
  cpp_args += ['-DWINVER=0x0601', '-D_WIN32_WINNT=0x0601']

  if meson.get_compiler('cpp').get_id() == 'msvc'
    cpp_args += ['/utf-8']
  endif
endif

executable(
  'WxClash',
  [
    'src/main.cpp',
    'src/config/app_config.cpp',
    'src/api/http_transport.cpp',
    'src/api/mihomo_api_client.cpp',
    'src/api/mihomo_api_service.cpp',
    'src/core/mihomo_sidecar.cpp',
    'src/ui/main_frame.cpp',
    'src/ui/pages.cpp',
  ],
  dependencies: [wx_dep, yaml_cpp_dep, nlohmann_json_dep],
  cpp_args: cpp_args,
  win_subsystem: 'windows',
  install: false,
)
