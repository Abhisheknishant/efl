ecore_file_deps = [ecore_con, ecore]
ecore_file_pub_deps = [eina]
ecore_file_ext_deps = []

ecore_file_src = files([
  'ecore_file.c',
  'ecore_file_download.c',
  'ecore_file_monitor.c',
  'ecore_file_path.c',
  'ecore_file_private.h'
])

if sys_windows == true
  ecore_file_src += files([ 'ecore_file_monitor_win32.c'])
elif sys_linux == true
  ecore_file_src += files([ 'ecore_file_monitor_inotify.c'])
else
  ecore_file_src += files([ 'ecore_file_monitor_poll.c'])
endif

ecore_file_header_src = ['Ecore_File.h']


ecore_file_lib = library('ecore_file',
    ecore_file_src,
    c_args : package_c_args,
    dependencies: ecore_file_deps + ecore_file_pub_deps + ecore_file_ext_deps,
    include_directories : config_dir + [include_directories(join_paths('..','..'))],
    install: true,
    version : meson.project_version()
)

ecore_file = declare_dependency(
  include_directories: [include_directories('.')],
  link_with: ecore_file_lib,
  dependencies: ecore_file_pub_deps,
)

install_headers(ecore_file_header_src,
  install_dir : dir_package_include,
)
