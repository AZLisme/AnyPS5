include(${CMAKE_CURRENT_SOURCE_DIR}/AvPlayerLibrary.cmake)
add_sce_avplayer_library(libSceAvPlayer)

if(BUILD_TESTING)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
    add_test_executable(guest_avplayer_tests ${LIBS_INCLUDE_DIR}/tests/GuestAvPlayer.cpp)
    target_include_directories(guest_avplayer_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_avplayer_tests PRIVATE libSceAvPlayer libc)
    configure_windows_unwind(guest_avplayer_tests)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guest_avplayer)
    add_test(NAME guest_avplayer COMMAND guest_avplayer_tests)
    add_test(NAME guest_avplayer_callback_close COMMAND guest_avplayer_tests --callback-close)
    add_test(NAME guest_avplayer_paused_first_video COMMAND guest_avplayer_tests --paused-first-video)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test_executable(avplayer_http_transport_tests ${LIBS_INCLUDE_DIR}/tests/AvPlayerHttpTransport.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/HttpStream.cpp)
        target_include_directories(avplayer_http_transport_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(avplayer_http_transport_tests PRIVATE ffmpeg libSceHttp libc)
        configure_windows_unwind(avplayer_http_transport_tests)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guest_avplayer_http)
        add_test(NAME guest_avplayer_http COMMAND ${Python3_EXECUTABLE}
            ${LIBS_INCLUDE_DIR}/tests/AvPlayerHttpServer.py $<TARGET_FILE:guest_avplayer_tests>)
        set_tests_properties(guest_avplayer_http PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guest_avplayer_http TIMEOUT 20)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/avplayer_http_transport)
        add_test(NAME avplayer_http_transport COMMAND ${Python3_EXECUTABLE}
            ${LIBS_INCLUDE_DIR}/tests/AvPlayerHttpServer.py $<TARGET_FILE:avplayer_http_transport_tests> transport)
        set_tests_properties(avplayer_http_transport PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/avplayer_http_transport TIMEOUT 10)
        if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
            add_test_executable(avplayer_http_patched_modules_tests ${LIBS_INCLUDE_DIR}/tests/AvPlayerHttpModules.cpp)
            target_link_libraries(avplayer_http_patched_modules_tests PRIVATE ${CMAKE_DL_LIBS})
            configure_windows_unwind(avplayer_http_patched_modules_tests)
            set(httpPatchedDependencies
                ${PATCHED_DIR}/libc.prx
                ${PATCHED_DIR}/libkernel.prx
                ${PATCHED_DIR}/libSceLibcInternal.prx
                ${PATCHED_DIR}/libSceHttp.prx
                ${PATCHED_DIR}/libSceAvPlayer.prx
                ${PATCHED_DIR}/libSceAvPlayer.native.prx)
            if(WIN32 AND TARGET mimalloc)
                list(APPEND httpPatchedDependencies
                    ${PATCHED_DIR}/mimalloc.dll
                    ${PATCHED_DIR}/mimalloc-redirect.dll)
            endif()
            add_custom_target(avplayer_http_patched_closure DEPENDS ${httpPatchedDependencies})
            add_dependencies(avplayer_http_patched_modules_tests avplayer_http_patched_closure
                guest_http_client_tests guest_avplayer_tests nid_patcher)
            add_test(NAME avplayer_http_patched_modules COMMAND ${CMAKE_COMMAND}
                -DPYTHON=${Python3_EXECUTABLE}
                -DSERVER=${LIBS_INCLUDE_DIR}/tests/AvPlayerHttpServer.py
                -DPATCHER=$<TARGET_FILE:nid_patcher>
                -DPATCHED_DIRECTORY=${PATCHED_DIR}
                -DOBJDUMP=${CMAKE_OBJDUMP}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/avplayer_http_patched_modules
                -P ${LIBS_INCLUDE_DIR}/tests/AvPlayerHttpModules.cmake
                -- "$<TARGET_FILE:avplayer_http_patched_modules_tests>"
                "$<TARGET_FILE:guest_http_client_tests>" "$<TARGET_FILE:guest_avplayer_tests>")
            set_tests_properties(avplayer_http_patched_modules PROPERTIES TIMEOUT 30)
        endif()
    endif()
    set_tests_properties(guest_avplayer_paused_first_video PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guest_avplayer_paused_first_video
        TIMEOUT 20)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guest_avplayer_paused_first_video)
    set_tests_properties(guest_avplayer PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guest_avplayer
        TIMEOUT 60)
    set_tests_properties(guest_avplayer_callback_close PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guest_avplayer
        TIMEOUT 20 RUN_SERIAL TRUE)
    if(WIN32)
        if(TEST guest_avplayer_http)
            set_tests_properties(guest_avplayer_http avplayer_http_transport PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAvPlayer>")
        endif()
        set_tests_properties(guest_avplayer PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAvPlayer>")
        set_tests_properties(guest_avplayer_paused_first_video PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAvPlayer>")
        set_tests_properties(guest_avplayer_callback_close PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAvPlayer>")
    endif()
endif()
