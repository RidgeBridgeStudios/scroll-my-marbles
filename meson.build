project(
  'scroll-my-marbles',
  'c',
  version: '1.0.0',
  license: 'MIT',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'buildtype=debugoptimized',
  ],
)

cc = meson.get_compiler('c')

# Project dependencies
glib_dep = dependency('glib-2.0', version: '>= 2.66')
gio_dep = dependency('gio-2.0', version: '>= 2.66')
gio_unix_dep = dependency('gio-unix-2.0', version: '>= 2.66')
libevdev_dep = dependency('libevdev', version: '>= 1.5')
gtk4_dep = dependency('gtk4', version: '>= 4.6')
adw_dep = dependency('libadwaita-1', version: '>= 1.2')
threads_dep = dependency('threads')

src_files = files(
  'src/main.c',
  'src/config/config.c',
  'src/core/device.c',
  'src/core/scroll_engine.c',
  'src/core/worker.c',
  'src/ui/settings_window.c',
  'src/ui/tray.c',
)

inc_dirs = include_directories('src')

executable(
  'scroll-my-marbles',
  src_files,
  include_directories: inc_dirs,
  dependencies: [
    glib_dep,
    gio_dep,
    gio_unix_dep,
    libevdev_dep,
    gtk4_dep,
    adw_dep,
    threads_dep,
  ],
  install: true,
)

# Desktop entry file
install_data(
  'data/scroll-my-marbles.desktop',
  install_dir: get_option('datadir') / 'applications',
)

# Udev rule
install_data(
  'data/99-scroll-my-marbles.rules',
  install_dir: '/lib/udev/rules.d',
)

# Icons
icon_sizes = ['16x16', '32x32', '48x48', '64x64', '128x128', '256x256', '512x512']
foreach s : icon_sizes
  install_data(
    'data/icons/hicolor' / s / 'apps/scroll-my-marbles.png',
    install_dir: get_option('datadir') / 'icons/hicolor' / s / 'apps',
  )
endforeach

install_data(
  'data/icons/hicolor/scalable/apps/scroll-my-marbles.svg',
  install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps',
)

# Unit Tests
test_scroll_engine = executable(
  'test_scroll_engine',
  files('tests/test_scroll_engine.c', 'src/core/scroll_engine.c'),
  include_directories: inc_dirs,
  dependencies: [libevdev_dep],
)

test('scroll_engine_tests', test_scroll_engine)
