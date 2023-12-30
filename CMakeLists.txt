cmake_minimum_required(VERSION 3.15)
project(middle)

add_executable(middle WIN32
    Middle.cpp

    main.cpp
    helpers.cpp
    middle.rc
)
target_compile_features(middle PRIVATE
    cxx_std_17
)
