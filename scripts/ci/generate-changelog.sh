#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:?Usage: generate-changelog.sh <version> [output.md]}"
OUTPUT="${2:-RELEASE_NOTES.md}"

PREV="$(git tag -l 'v*' --sort=-version:refname | head -n1 || true)"

{
  echo "# Arachnel v${VERSION}"
  echo
  echo "## Changes"
  echo
  if [[ -z "${PREV}" ]]; then
    git log --pretty=format:'- %s (%h)' --no-merges
  else
    echo "Since ${PREV}:"
    echo
    git log "${PREV}..HEAD" --pretty=format:'- %s (%h)' --no-merges
  fi
  echo
  echo
  echo "## Downloads"
  echo
  echo "- **Windows:** \`Arachnel-${VERSION}-Setup.exe\`"
  echo "- **Linux:** \`Arachnel-${VERSION}-x86_64.AppImage\`"
  echo
  echo "Verify checksums in \`checksums.sha256\`."
} >"${OUTPUT}"

echo "Wrote ${OUTPUT}"
