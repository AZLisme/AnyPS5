add_library(libSceIcu SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Utf8.cpp ${CMAKE_CURRENT_SOURCE_DIR}/IteratorCompare.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TextTraversal.cpp)
target_include_directories(libSceIcu PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceIcu)

if(BUILD_TESTING)
    foreach(kind Utf8 IteratorCompare TextTraversal)
        string(TOLOWER ${kind} suffix)
        add_test_executable(guest_icu_${suffix}_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/${kind}.cpp)
        target_include_directories(guest_icu_${suffix}_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(guest_icu_${suffix}_tests PRIVATE libSceIcu)
        configure_windows_unwind(guest_icu_${suffix}_tests)
        add_test(NAME guest_icu_${suffix} COMMAND guest_icu_${suffix}_tests)
        set_tests_properties(guest_icu_${suffix} PROPERTIES TIMEOUT 20)
        if(WIN32)
            set_tests_properties(guest_icu_${suffix} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
        endif()
    endforeach()
endif()
