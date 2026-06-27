function(link_package NAME)
    cmake_parse_arguments(LP "ENCOURAGED" "TARGET" "" ${ARGN})

    if(${NAME}_FOUND)
        string(TOUPPER "${NAME}" UCNAME)
        # Some libraries have a dash, which is not allowed in a preprocessor macro.
        string(REPLACE "-" "_" UCNAME "${UCNAME}")

        add_definitions(-DWITH_${UCNAME})
        string(STRIP "${${NAME}_LIBRARY}" ${NAME}_LIBRARY)
        string(STRIP "${${NAME}_LIBRARIES}" ${NAME}_LIBRARIES)
        include_directories(${${NAME}_INCLUDE_DIRS} ${${NAME}_INCLUDE_DIR})
        target_link_libraries(openttd_lib ${${NAME}_LIBRARIES} ${${NAME}_LIBRARY})
        message(STATUS "${NAME} found -- -DWITH_${UCNAME} -- ${${NAME}_INCLUDE_DIRS} ${${NAME}_INCLUDE_DIR} -- ${${NAME}_LIBRARIES} ${${NAME}_LIBRARY}")
    elseif(LP_ENCOURAGED)
        message(WARNING "${NAME} not found; compiling OpenTTD without ${NAME} is strongly discouraged")
    endif()
endfunction()
