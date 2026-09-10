# Qt 6.8's FindWrapOpenGL links AGL.framework on macOS, either as a path found on
# the running system or as a bare "-framework AGL" fallback. Apple removed AGL
# from the macOS 26 SDK, so the link fails against that SDK even though nothing
# here uses AGL. Strip it from the wrapper target. Harmless on older SDKs, where
# the framework is found and simply not needed.
#
# Included by the plugin build and by the standalone unit test build.

include_guard(GLOBAL)

function(fix_qt_wrapopengl_agl)
  if(NOT APPLE OR NOT TARGET WrapOpenGL::WrapOpenGL)
    return()
  endif()
  get_target_property(_wrap_opengl_libs WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
  if(_wrap_opengl_libs)
    list(FILTER _wrap_opengl_libs EXCLUDE REGEX "(^-framework AGL$|/AGL\\.framework$)")
    set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES INTERFACE_LINK_LIBRARIES "${_wrap_opengl_libs}")
  endif()
endfunction()
