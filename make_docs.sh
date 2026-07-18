#!/usr/bin/env bash
set -euo pipefail

SRC_DIR="${1:-src}"
OUT_DIR="docs"

if [[ ! -f Doxyfile ]]; then
  cat > Doxyfile <<'DOXY'
PROJECT_NAME           = "Hyperverse"
OUTPUT_DIRECTORY       = docs
INPUT                  = src include
RECURSIVE              = YES
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
MARKDOWN_SUPPORT       = YES
EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = NO
QUIET                  = YES
WARN_IF_UNDOCUMENTED   = NO
WARN_AS_ERROR          = NO
SOURCE_BROWSER         = YES
INLINE_SOURCES         = NO
HAVE_DOT               = NO
DOXY
fi

echo "Generating API docs from ${SRC_DIR} into ${OUT_DIR}/html"
doxygen Doxyfile
echo "Done: ${OUT_DIR}/html/index.html"
