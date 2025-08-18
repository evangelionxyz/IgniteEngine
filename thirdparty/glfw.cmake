# GLFW (handpicked sources per premake)
add_library(GLFW STATIC
  GLFW/src/context.c
  GLFW/src/init.c
  GLFW/src/input.c
  GLFW/src/monitor.c
  GLFW/src/null_init.c
  GLFW/src/null_joystick.c
  GLFW/src/null_monitor.c
  GLFW/src/null_window.c
  GLFW/src/platform.c
  GLFW/src/window.c
  GLFW/src/vulkan.c
  GLFW/src/osmesa_context.c
  GLFW/src/wgl_context.c
  GLFW/src/egl_context.c
)
target_include_directories(GLFW PUBLIC ${THIRDPARTY_DIR}/GLFW/include)
if(WIN32)
  target_sources(GLFW PRIVATE
    GLFW/src/win32_init.c
    GLFW/src/win32_joystick.c
    GLFW/src/win32_monitor.c
    GLFW/src/win32_module.c
    GLFW/src/win32_time.c
    GLFW/src/win32_thread.c
    GLFW/src/win32_window.c
  )
  target_compile_definitions(GLFW PUBLIC _GLFW_WIN32 _CRT_SECURE_NO_WARNINGS)
else()
  target_sources(GLFW PRIVATE
    GLFW/src/posix_time.c
    GLFW/src/posix_thread.c
    GLFW/src/posix_poll.c
    GLFW/src/posix_module.c
    GLFW/src/linux_joystick.c
    GLFW/src/x11_init.c
    GLFW/src/x11_monitor.c
    GLFW/src/x11_window.c
    GLFW/src/xkb_unicode.c
  )
  target_compile_definitions(GLFW PUBLIC _GLFW_X11)
endif()
set_common_target_options(GLFW)