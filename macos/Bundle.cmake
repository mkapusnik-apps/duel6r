cmake_minimum_required(VERSION 3.16)
include(BundleUtilities)
set(BU_CHMOD_BUNDLE_ITEMS TRUE)
set(BU_COPY_FULL_FRAMEWORK_CONTENTS TRUE)

function(gp_item_default_embedded_path_override item path)
    set(${path} "@executable_path/../Frameworks" PARENT_SCOPE)
endfunction()

# Record original dependencies before relocation for license/provenance copying.
get_bundle_keys("${APP}" "${EXTRA_LIBS}" "${LIB_DIRS}" keys)
file(WRITE "${DEPENDENCY_LIST}" "")
foreach(key IN LISTS keys)
    file(APPEND "${DEPENDENCY_LIST}" "${${key}_RESOLVED_ITEM}\n")
endforeach()
clear_bundle_keys(keys)
fixup_bundle("${APP}" "${EXTRA_LIBS}" "${LIB_DIRS}")
