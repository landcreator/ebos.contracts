add_contract(transrecords transrecords ${CMAKE_CURRENT_SOURCE_DIR}/src/transrecords.cpp)

target_include_directories(transrecords
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(transrecords
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")


target_compile_options( transrecords PUBLIC)