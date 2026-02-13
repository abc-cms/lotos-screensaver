# Root meson.build file for lotos-screensaver project

project('lotos-screensaver', 'cpp',
  version: '1.0.0',
  meson_version: '>= 0.55.0'
)

# Set default C++ standard
cpp_std = 'c++17'

# Define common include directory
common_inc = include_directories('screensaver/common')

# Screensaver executable
screensaver_dep = dependency('sdl2', required: true)
mpv_dep = dependency('mpv', required: true)
gl_dep = dependency('gl', required: true)
glew_dep = dependency('glew', required: true)
sdl2_ttf_dep = dependency('sdl2_ttf', required: true)

executable('lotos-screensaver',
  'screensaver/screensaver/screensaver.cpp',
  'screensaver/screensaver/button.cpp',
  include_directories: common_inc,
  dependencies: [screensaver_dep, mpv_dep, gl_dep, glew_dep, sdl2_ttf_dep],
  install: true,
  install_dir: '/usr/bin'
)

# Idle daemon executable
idle_dep = dependency('libinput', version: '1.0')
udev_dep = dependency('libudev')
json_dep = dependency('nlohmann_json')
filesystem_dep = dependency('filesystem', required: false)

executable('lotos-idle',
  'screensaver/idle/idle.cpp',
  'screensaver/idle/idle_daemon.cpp',
  include_directories: [common_inc],
  dependencies: [idle_dep, udev_dep, json_dep, filesystem_dep],
  install: true,
  install_dir: '/usr/bin'
)
