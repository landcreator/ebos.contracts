add_contract(debts debts ${CMAKE_CURRENT_SOURCE_DIR}/src/debts.cpp)

target_include_directories(debts
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(debts
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")


target_compile_options( debts PUBLIC)