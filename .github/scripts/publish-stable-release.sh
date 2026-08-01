#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 4 ]]; then
  echo "Usage: $0 <repository> <master-commit> <archive> <provenance>" >&2
  exit 2
fi

repository="$1"
master_commit="$2"
archive="$3"
provenance="$4"
stable_tag="released-${master_commit}"
archive_name="$(basename "${archive}")"
provenance_name="$(basename "${provenance}")"

[[ "${master_commit}" =~ ^[0-9a-f]{40}$ ]]
[[ -f "${archive}" ]]
[[ -f "${provenance}" ]]

get_release() {
  gh api --paginate --slurp "repos/${repository}/releases?per_page=100" |
    jq -ce --arg tag "${stable_tag}" '
      [.[][] | select(.tag_name == $tag)] |
      if length == 1 then .[0]
      elif length == 0 then null
      else error("multiple releases use the immutable stable tag")
      end
    '
}

verify_existing_tag() {
  gh api "repos/${repository}/git/matching-refs/tags/${stable_tag}" |
    jq -e --arg ref "refs/tags/${stable_tag}" --arg sha "${master_commit}" '
      [.[] | select(.ref == $ref)] |
      length == 0 or (length == 1 and .[0].object.sha == $sha)
    ' >/dev/null
}

verify_release() {
  local release_json="$1"
  local expected_draft="$2"
  local release_id archive_id provenance_id download_dir

  jq -e \
    --arg tag "${stable_tag}" \
    --arg target "${master_commit}" \
    --arg archive "${archive_name}" \
    --arg provenance "${provenance_name}" \
    --argjson draft "${expected_draft}" '
      .tag_name == $tag and
      .target_commitish == $target and
      .draft == $draft and
      .prerelease == false and
      ([.assets[].name] | sort) == ([$archive, $provenance] | sort)
    ' <<< "${release_json}" >/dev/null

  release_id="$(jq -er '.id' <<< "${release_json}")"
  archive_id="$(jq -er --arg name "${archive_name}" '.assets[] | select(.name == $name) | .id' <<< "${release_json}")"
  provenance_id="$(jq -er --arg name "${provenance_name}" '.assets[] | select(.name == $name) | .id' <<< "${release_json}")"
  download_dir="$(mktemp -d)"
  gh api -H "Accept: application/octet-stream" \
    "repos/${repository}/releases/assets/${archive_id}" > "${download_dir}/${archive_name}"
  gh api -H "Accept: application/octet-stream" \
    "repos/${repository}/releases/assets/${provenance_id}" > "${download_dir}/${provenance_name}"
  cmp "${archive}" "${download_dir}/${archive_name}"
  cmp "${provenance}" "${download_dir}/${provenance_name}"
  rm -rf "${download_dir}"
  printf '%s\n' "${release_id}"
}

verify_existing_tag
release_json="$(get_release)"
if [[ "${release_json}" != "null" && "$(jq -r '.draft' <<< "${release_json}")" == "true" ]]; then
  gh api --method DELETE "repos/${repository}/releases/$(jq -er '.id' <<< "${release_json}")"
  release_json="null"
fi

if [[ "${release_json}" == "null" ]]; then
  gh release create "${stable_tag}" "${archive}" "${provenance}" \
    --repo "${repository}" \
    --target "${master_commit}" \
    --title "Duel 6 Reloaded ${master_commit:0:12}" \
    --notes "Exact provenance-verified nightly bytes promoted for master ${master_commit}." \
    --draft
  release_json="$(get_release)"
fi

if [[ "$(jq -r '.draft' <<< "${release_json}")" == "true" ]]; then
  release_id="$(verify_release "${release_json}" true)"
  gh api --method PATCH "repos/${repository}/releases/${release_id}" \
    -F draft=false -F prerelease=false >/dev/null
  release_json="$(get_release)"
fi

verify_release "${release_json}" false >/dev/null
test "$(gh api "repos/${repository}/git/ref/tags/${stable_tag}" --jq .object.sha)" = "${master_commit}"
echo "Published and verified immutable stable release ${stable_tag}"
