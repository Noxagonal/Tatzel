cmake_minimum_required(VERSION 4.0)

find_program(NODE_EXECUTABLE node REQUIRED)
find_program(NPM_EXECUTABLE npm REQUIRED)

set(NPM_STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/npm_install.stamp")
set(TYPESCRIPT_STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/typescript_build.stamp")
set(TYPESCRIPT_COMPILER "${Tatzel_SOURCE_DIR}/node_modules/.bin/tsc")
if(WIN32)
	set(TYPESCRIPT_COMPILER "${TYPESCRIPT_COMPILER}.cmd")
endif()

file(GLOB_RECURSE TYPESCRIPT_SOURCE_FILES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.ts"
)

set(TYPESCRIPT_OUTPUT_FILES)
foreach(SOURCE_FILE IN LISTS TYPESCRIPT_SOURCE_FILES)
	file(RELATIVE_PATH RELATIVE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${SOURCE_FILE}")
	string(REGEX REPLACE "\\.ts$" ".js" RELATIVE_OUTPUT "${RELATIVE_SOURCE}")
	list(APPEND TYPESCRIPT_OUTPUT_FILES
		"${Tatzel_SOURCE_DIR}/www/app/${RELATIVE_OUTPUT}"
		"${Tatzel_SOURCE_DIR}/www/app/${RELATIVE_OUTPUT}.map"
	)
endforeach()

add_custom_command(
	OUTPUT "${NPM_STAMP_FILE}" "${TYPESCRIPT_COMPILER}"
	COMMAND "${NPM_EXECUTABLE}" ci
	COMMAND "${CMAKE_COMMAND}" -E touch "${NPM_STAMP_FILE}"
	WORKING_DIRECTORY "${Tatzel_SOURCE_DIR}"
	DEPENDS "${Tatzel_SOURCE_DIR}/package.json" "${Tatzel_SOURCE_DIR}/package-lock.json"
	COMMENT "Installing locked browser build dependencies"
	VERBATIM
	USES_TERMINAL
)

add_custom_target(npm_install DEPENDS "${NPM_STAMP_FILE}" "${TYPESCRIPT_COMPILER}")

add_custom_command(
	OUTPUT "${TYPESCRIPT_STAMP_FILE}" ${TYPESCRIPT_OUTPUT_FILES}
	COMMAND "${NPM_EXECUTABLE}" run build
	COMMAND "${CMAKE_COMMAND}" -E touch "${TYPESCRIPT_STAMP_FILE}"
	WORKING_DIRECTORY "${Tatzel_SOURCE_DIR}"
	DEPENDS
		"${NPM_STAMP_FILE}"
		"${TYPESCRIPT_COMPILER}"
		"${Tatzel_SOURCE_DIR}/package.json"
		"${Tatzel_SOURCE_DIR}/package-lock.json"
		"${Tatzel_SOURCE_DIR}/tsconfig.json"
		${TYPESCRIPT_SOURCE_FILES}
	COMMENT "Compiling browser DOM and event code"
	VERBATIM
	USES_TERMINAL
)

add_custom_target(typescript_build DEPENDS "${TYPESCRIPT_STAMP_FILE}" ${TYPESCRIPT_OUTPUT_FILES})
