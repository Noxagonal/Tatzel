find_package(SQLite3 REQUIRED)
if(TARGET SQLite3::SQLite3)
	set(_TATZEL_SQLITE_TARGET SQLite3::SQLite3)
else()
	set(_TATZEL_SQLITE_TARGET SQLite::SQLite3)
endif()

add_library(TatzelWorkspaceStore STATIC private/Repository.cpp)
target_include_directories(TatzelWorkspaceStore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_link_libraries(TatzelWorkspaceStore PUBLIC Tatzel ${_TATZEL_SQLITE_TARGET})

add_executable(TatzelWorkspace private/Workspace.cpp)
target_link_libraries(TatzelWorkspace PRIVATE TatzelWorkspaceStore)

if(TATZEL_BUILD_TESTS)
	add_executable(TatzelWorkspaceStoreTests "${Tatzel_SOURCE_DIR}/tests/cpp/WorkspaceStore.cpp")
	target_link_libraries(TatzelWorkspaceStoreTests PRIVATE TatzelWorkspaceStore)
	add_test(NAME TatzelWorkspaceStoreTests COMMAND TatzelWorkspaceStoreTests)
endif()
