include_guard(GLOBAL)

#[[!
@brief Prepare private native libraries without modifying the embedding project's defaults.
@pre A compiler is selected; make, OpenSSL, PIC static zlib, and UUID development files are available.
@post The default backend uses static Drogon, Trantor, and JsonCpp; dependency install rules are excluded.
]]
function(tatzel_prepare_dependencies)
	if(TARGET Drogon::Drogon)
		return()
	endif()
	if(TATZEL_USE_SYSTEM_DROGON)
		find_package(Drogon 1.9.13 CONFIG REQUIRED)
		return()
	endif()

	include(FetchContent)
	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
	set(BUILD_SHARED_LIBS OFF)
	set(BUILD_STATIC_LIBS ON)
	set(BUILD_OBJECT_LIBS OFF)
	set(JSONCPP_WITH_TESTS OFF)
	set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF)
	set(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF)
	set(JSONCPP_WITH_CMAKE_PACKAGE OFF)
	FetchContent_Declare(jsoncpp
		GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
		GIT_TAG 1.9.6 GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
	FetchContent_MakeAvailable(jsoncpp)

	# Drogon's finder accepts an existing CMake library target as its link item.
	set(JSONCPP_LIBRARIES jsoncpp_static)
	set(JSONCPP_INCLUDE_DIRS "${jsoncpp_SOURCE_DIR}/include")
	find_library(_TATZEL_ZLIB_ARCHIVE NAMES libz.a REQUIRED)
	find_library(_TATZEL_UUID_ARCHIVE NAMES libuuid.a REQUIRED)
	set(ZLIB_LIBRARY "${_TATZEL_ZLIB_ARCHIVE}")
	set(ZLIB_LIBRARY_RELEASE "${_TATZEL_ZLIB_ARCHIVE}")
	set(UUID_LIBRARY "${_TATZEL_UUID_ARCHIVE}")
	set(OPENSSL_USE_STATIC_LIBS FALSE)
	set(TRANTOR_USE_TLS openssl)
	set(BUILD_C-ARES OFF)
	set(BUILD_CTL OFF)
	set(BUILD_EXAMPLES OFF)
	set(BUILD_ORM OFF)
	set(BUILD_BROTLI OFF)
	set(BUILD_YAML_CONFIG OFF)
	set(BUILD_TESTING OFF)
	FetchContent_Declare(drogon
		GIT_REPOSITORY https://github.com/drogonframework/drogon.git
		GIT_TAG v1.9.13 GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
	FetchContent_MakeAvailable(drogon)

	# Distribution UUID archives may use non-PIC thread-local storage. Build the
	# upstream library with PIC; the system UUID package is used only by Drogon's
	# configure-time header/API probes, before replacing its link implementation.
	FetchContent_Declare(tatzel_uuid
		URL https://www.kernel.org/pub/linux/utils/util-linux/v2.40/util-linux-2.40.2.tar.xz
		URL_HASH SHA256=d78b37a66f5922d70edf3bdfb01a6b33d34ed3c3cafd6628203b2a2b67c8e8b3
		DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
	FetchContent_MakeAvailable(tatzel_uuid)
	include(ExternalProject)
	find_program(_TATZEL_MAKE_EXECUTABLE NAMES gmake make REQUIRED)
	set(_UUID_BUILD_DIRECTORY "${Tatzel_BINARY_DIR}/_deps/uuid-build")
	ExternalProject_Add(tatzel_uuid_build
		SOURCE_DIR "${tatzel_uuid_SOURCE_DIR}"
		BINARY_DIR "${_UUID_BUILD_DIRECTORY}"
		DOWNLOAD_COMMAND "" UPDATE_COMMAND ""
		CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
			"${tatzel_uuid_SOURCE_DIR}/configure"
			--disable-all-programs --enable-libuuid --disable-shared --enable-static
			--with-pic --disable-nls --without-python --without-systemd
		BUILD_COMMAND "${_TATZEL_MAKE_EXECUTABLE}" libuuid.la
		INSTALL_COMMAND ""
		BUILD_BYPRODUCTS "${_UUID_BUILD_DIRECTORY}/.libs/libuuid.a"
		EXCLUDE_FROM_ALL TRUE)
	add_library(TatzelBundledUUID STATIC IMPORTED GLOBAL)
	set_target_properties(TatzelBundledUUID PROPERTIES
		IMPORTED_LOCATION "${_UUID_BUILD_DIRECTORY}/.libs/libuuid.a"
		INTERFACE_INCLUDE_DIRECTORIES "${UUID_INCLUDE_DIR}")
	add_dependencies(TatzelBundledUUID tatzel_uuid_build)
	foreach(_PROPERTY LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
		get_target_property(_LINK_ITEMS drogon "${_PROPERTY}")
		string(REPLACE "UUID_lib" "TatzelBundledUUID" _LINK_ITEMS "${_LINK_ITEMS}")
		set_property(TARGET drogon PROPERTY "${_PROPERTY}" "${_LINK_ITEMS}")
	endforeach()

	set(TATZEL_DROGON_SOURCE_DIRECTORY "${drogon_SOURCE_DIR}" PARENT_SCOPE)
	set(TATZEL_JSONCPP_SOURCE_DIRECTORY "${jsoncpp_SOURCE_DIR}" PARENT_SCOPE)
endfunction()
