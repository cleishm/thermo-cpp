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

# The formatters must also build with exceptions disabled, as on embedded
# targets. Catch2 requires exceptions, so this is a standalone target. MSVC
# spells the option differently and still declares __cpp_exceptions, so the
# check is limited to compilers taking -fno-exceptions.
if(NOT MSVC)
    add_executable(thermo_no_exceptions_test no_exceptions_test.cpp)
    target_link_libraries(thermo_no_exceptions_test PRIVATE thermo::thermo)
    target_compile_options(thermo_no_exceptions_test PRIVATE -fno-exceptions)
    add_test(NAME no_exceptions COMMAND thermo_no_exceptions_test)
endif()
