add_executable(TatzelCoreTests LogicalCore.cpp)
target_link_libraries(TatzelCoreTests PRIVATE Tatzel)
add_test(NAME TatzelCoreTests COMMAND TatzelCoreTests)

add_executable(TatzelControlTests Controls.cpp)
target_link_libraries(TatzelControlTests PRIVATE Tatzel)
add_test(NAME TatzelControlTests COMMAND TatzelControlTests)

add_executable(TatzelFormTests Forms.cpp)
target_link_libraries(TatzelFormTests PRIVATE Tatzel)
add_test(NAME TatzelFormTests COMMAND TatzelFormTests)
