executable('lotos_idle',
  sources: ['idle.cpp'],
  dependencies: [
    dependency('libinput'),
    dependency('libudev'),
    dependency('nlohmann_json'),
  ],
  include_directories: include_directories('.'),
  install: true,
  install_dir: '/usr/bin'
)
