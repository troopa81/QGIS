# TODO Cartouche

add_executable(typesystem_generator main.cpp)

target_include_directories(typesystem_generator
    PRIVATE ${CMAKE_SOURCE_DIR}/external/shiboken_apiextractor
)

target_link_libraries(typesystem_generator
    PRIVATE
        apiextractor
        Qt::Core
)
