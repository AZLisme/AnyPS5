add_library(libSceIcu SHARED EXCLUDE_FROM_ALL Export.cpp Tokenize.cpp)
target_include_directories(libSceIcu PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceIcu)

if(BUILD_TESTING)
    add_test_executable(guest_icu_tokenize_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuTokenize.cpp)
    target_include_directories(guest_icu_tokenize_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_tokenize_tests PRIVATE libSceIcu libkernel libc)
    configure_windows_unwind(guest_icu_tokenize_tests)
    add_test(NAME guest_icu_tokenize COMMAND guest_icu_tokenize_tests)
    set_tests_properties(guest_icu_tokenize PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_tokenize PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>"
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
endif()
