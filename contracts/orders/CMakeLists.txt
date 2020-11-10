add_contract(orders orders ${CMAKE_CURRENT_SOURCE_DIR}/src/orders.cpp)

target_include_directories(orders
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(orders
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")


target_compile_options( orders PUBLIC)