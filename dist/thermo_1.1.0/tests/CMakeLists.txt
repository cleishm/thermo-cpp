include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

add_executable(thermo_tests
    thermo_test.cpp
)

target_link_libraries(thermo_tests PRIVATE
    thermo::thermo
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(thermo_tests)
