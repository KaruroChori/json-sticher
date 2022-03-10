# Typically you don't care so much for a third party library's tests to be
# run from your own project's code.
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(json)

set(CPR_BUILD_TESTS OFF CACHE INTERNAL "")
add_subdirectory(cpr)

set(JSON_VALIDATOR_BUILD_TESTS OFF CACHE INTERNAL "")
set(JSON_VALIDATOR_BUILD_EXAMPLES OFF CACHE INTERNAL "")
add_subdirectory(json-schema-validator)

add_subdirectory(base64)

add_subdirectory(cxxopts)
