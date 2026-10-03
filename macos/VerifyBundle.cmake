cmake_minimum_required(VERSION 3.16)
include(BundleUtilities)
# verify_app alone can report success after scanning zero executables when its
# input is invalid. Require the declared main executable before checking closure.
get_bundle_and_executable("${APP}" bundle executable valid)
if(NOT valid)
    get_bundle_main_executable("${APP}" reason)
    message(FATAL_ERROR "Invalid application bundle: ${reason}")
endif()
verify_app("${APP}")
