#!/usr/bin/env bash

# Records only full-frame screenshots produced by a graphical test. Diagnostic
# collection consumes this provenance instead of trying to infer image purpose
# from filenames.

d6r_enable_screenshot_manifest() {
    D6R_SCREENSHOT_ROOT="$1"
    D6R_SCREENSHOT_MANIFEST="${D6R_SCREENSHOT_ROOT}/.original-screenshots"
    D6R_IMAGEMAGICK_IMPORT="$(type -P import)"
    : >"${D6R_SCREENSHOT_MANIFEST}"

    # Called by the sourcing graphical harnesses after this function returns.
    # shellcheck disable=SC2329
    import() {
        "${D6R_IMAGEMAGICK_IMPORT}" "$@"
        local destination="${!#}"
        case "$destination" in
            *.png) d6r_record_full_screenshot "$destination" ;;
        esac
    }
}

d6r_record_full_screenshot() {
    local screenshot="$1"
    case "$screenshot" in
        "${D6R_SCREENSHOT_ROOT}"/*.png)
            printf '%s\n' "${screenshot#"${D6R_SCREENSHOT_ROOT}/"}" \
                >>"${D6R_SCREENSHOT_MANIFEST}"
            ;;
        *)
            printf 'Refusing to record screenshot outside test root: %s\n' "$screenshot" >&2
            return 1
            ;;
    esac
}
