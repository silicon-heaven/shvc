if (
  demo_device_opt.enabled()
  or (demo_device_opt.auto()
  and not meson.is_subproject())
)
  subdir('device')
endif

if (
  demo_client_opt.enabled()
  or (demo_client_opt.auto()
  and not meson.is_subproject())
)
  subdir('client')
endif

if (
  demo_history_opt.enabled()
  or (demo_history_opt.auto()
  and not meson.is_subproject())
)
  subdir('history')
endif
