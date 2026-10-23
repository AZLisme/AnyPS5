add_library(libSceIcu SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HashPrimitives.cpp
)
target_include_directories(libSceIcu PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceIcu PRIVATE libc)
configure_windows_unwind(libSceIcu)

if(BUILD_TESTING)
    add_test_executable(guest_icu_hash_primitives_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/HashPrimitives.cpp)
    target_include_directories(guest_icu_hash_primitives_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_hash_primitives_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_hash_primitives_tests)
    add_test(NAME guest_icu_hash_primitives COMMAND guest_icu_hash_primitives_tests)
    set_tests_properties(guest_icu_hash_primitives PROPERTIES TIMEOUT 15)
    if(WIN32)
        set_tests_properties(guest_icu_hash_primitives PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()
