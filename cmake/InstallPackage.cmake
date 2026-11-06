include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(TATZEL_INSTALL_SOURCE_DIRECTORY "${CMAKE_INSTALL_DATADIR}/tatzel/source")
set(TATZEL_INSTALL_RUNTIME_DIRECTORY "${CMAKE_INSTALL_DATADIR}/tatzel/runtime")
set(TATZEL_INSTALL_CMAKE_DIRECTORY "${CMAKE_INSTALL_LIBDIR}/cmake/Tatzel")

configure_package_config_file("${Tatzel_SOURCE_DIR}/cmake/TatzelConfig.cmake.in"
	"${Tatzel_BINARY_DIR}/TatzelConfig.cmake"
	INSTALL_DESTINATION "${TATZEL_INSTALL_CMAKE_DIRECTORY}"
	PATH_VARS TATZEL_INSTALL_SOURCE_DIRECTORY TATZEL_INSTALL_RUNTIME_DIRECTORY
)
write_basic_package_version_file("${Tatzel_BINARY_DIR}/TatzelConfigVersion.cmake"
	VERSION "${Tatzel_VERSION}" COMPATIBILITY SameMinorVersion)

install(FILES "${Tatzel_BINARY_DIR}/TatzelConfig.cmake" "${Tatzel_BINARY_DIR}/TatzelConfigVersion.cmake"
	DESTINATION "${TATZEL_INSTALL_CMAKE_DIRECTORY}")
install(FILES
	"${Tatzel_SOURCE_DIR}/cmake/TatzelLibrary.cmake"
	"${Tatzel_SOURCE_DIR}/cmake/TatzelRuntime.cmake"
	"${Tatzel_SOURCE_DIR}/cmake/Stacktrace.cmake"
	"${Tatzel_SOURCE_DIR}/cmake/TargetOps.cmake"
	"${Tatzel_SOURCE_DIR}/cmake/FileOps.cmake"
	DESTINATION "${TATZEL_INSTALL_CMAKE_DIRECTORY}")
install(DIRECTORY "${Tatzel_SOURCE_DIR}/targets/tatzel/public" "${Tatzel_SOURCE_DIR}/targets/tatzel/private"
	DESTINATION "${TATZEL_INSTALL_SOURCE_DIRECTORY}")
install(DIRECTORY "${Tatzel_RUNTIME_DIRECTORY}/" DESTINATION "${TATZEL_INSTALL_RUNTIME_DIRECTORY}")
install(DIRECTORY "${Tatzel_SOURCE_DIR}/examples/installed" DESTINATION "${CMAKE_INSTALL_DATADIR}/tatzel/examples")
install(FILES "${Tatzel_SOURCE_DIR}/LICENSE" "${Tatzel_SOURCE_DIR}/docs/package.md"
	DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/Tatzel")
