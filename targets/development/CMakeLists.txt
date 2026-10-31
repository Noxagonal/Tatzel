cmake_minimum_required(VERSION 4.0)

include("${Tatzel_SOURCE_DIR}/cmake/TargetOps.cmake")


##############################################################
# Tatzel development executable target
##############################################################
add_executable(TatzelDevelopment)
set_default_target_files(TatzelDevelopment "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TatzelDevelopment
	PRIVATE
		Drogon::Drogon
		Tatzel
)
