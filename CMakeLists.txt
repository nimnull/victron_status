# Victron Multi-System Monitor
# Target: Waveshare ESP32-S3-Touch-AMOLED-2.41
cmake_minimum_required(VERSION 3.22)

# Parse secrets.txt and generate sdkconfig.defaults.secrets before project()
set(SECRETS_FILE "${CMAKE_CURRENT_LIST_DIR}/main/secrets.txt")
set(SECRETS_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.secrets")

if(EXISTS ${SECRETS_FILE})
    message(STATUS "Loading secrets from ${SECRETS_FILE}")
    file(STRINGS ${SECRETS_FILE} SECRETS_LINES)

    # Start with header
    set(SECRETS_CONTENT "# Auto-generated from main/secrets.txt - DO NOT EDIT\n")

    foreach(LINE ${SECRETS_LINES})
        # Skip empty lines and comments
        if(LINE MATCHES "^[^#].*=.*")
            # Extract key=value pairs
            string(REGEX MATCH "^([^=]+)=(.*)$" MATCH ${LINE})
            if(CMAKE_MATCH_1 AND CMAKE_MATCH_2)
                set(KEY ${CMAKE_MATCH_1})
                set(VALUE ${CMAKE_MATCH_2})
                # Strip whitespace
                string(STRIP "${KEY}" KEY)
                string(STRIP "${VALUE}" VALUE)
                # Convert to Kconfig format: CONFIG_KEY="value"
                set(SECRETS_CONTENT "${SECRETS_CONTENT}CONFIG_${KEY}=\"${VALUE}\"\n")
            endif()
        endif()
    endforeach()

    # Write the generated defaults file
    file(WRITE ${SECRETS_DEFAULTS} ${SECRETS_CONTENT})
    message(STATUS "Generated ${SECRETS_DEFAULTS}")
else()
    message(WARNING "Secrets file not found: ${SECRETS_FILE}")
    message(WARNING "Copy main/secrets.txt.example to main/secrets.txt and fill in your values")
    # Create empty file to prevent build errors
    if(NOT EXISTS ${SECRETS_DEFAULTS})
        file(WRITE ${SECRETS_DEFAULTS} "# No secrets configured\n")
    endif()
endif()

# Tell ESP-IDF to load both sdkconfig.defaults files
# Secrets file is loaded second so it overrides placeholder values
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults"
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.secrets"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Minimal build: only components required by main and its dependencies
idf_build_set_property(MINIMAL_BUILD ON)
project(victron-monitor)
