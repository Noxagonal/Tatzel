include("${Tatzel_SOURCE_DIR}/cmake/TatzelLibrary.cmake")
tatzel_add_library("${CMAKE_CURRENT_SOURCE_DIR}" "${Tatzel_RUNTIME_DIRECTORY}")
add_dependencies(Tatzel tatzel_runtime)
