project(
  'WxClash',
  'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++17',
    'b_vscrt=mt',
  ],
)

# wx-config supplies the platform-specific include paths and libraries. The
# application only needs the core, base, and networking components.
wx_dep = dependency(
  'wxwidgets',
  modules: ['core', 'base', 'net'],
  required: true,
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
    'src/api/mihomo_api_client.cpp',
    'src/core/mihomo_sidecar.cpp',
    'src/ui/main_frame.cpp',
  ],
  dependencies: wx_dep,
  cpp_args: cpp_args,
  win_subsystem: 'windows',
  install: false,
)
