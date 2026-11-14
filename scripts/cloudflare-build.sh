#
# CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
#
# Copyright (c) 2026. All rights reserved.
# Tuğrul Güngör - https://conll-u.tugrulgungor.me
#
# Distributed under the MIT License.
# https://opensource.org/license/mit/
#

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS="$ROOT/.tools"

DOXYGEN_VERSION="${DOXYGEN_VERSION:-1.12.0}"
EMSDK_VERSION="${EMSDK_VERSION:-latest}"

mkdir -p "$TOOLS"

# CMake + Ninja
python3 -m venv "$TOOLS/venv"
"$TOOLS/venv/bin/pip" install --quiet cmake ninja
export PATH="$TOOLS/venv/bin:$PATH"
export CMAKE_GENERATOR=Ninja

# Doxygen
if [ ! -x "$TOOLS/doxygen-$DOXYGEN_VERSION/bin/doxygen" ]; then
    curl -fsSL "https://github.com/doxygen/doxygen/releases/download/Release_${DOXYGEN_VERSION//./_}/doxygen-$DOXYGEN_VERSION.linux.bin.tar.gz" |
        tar -xz -C "$TOOLS"
fi
export PATH="$TOOLS/doxygen-$DOXYGEN_VERSION/bin:$PATH"

# Emscripten
if [ ! -d "$TOOLS/emsdk" ]; then
    git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$TOOLS/emsdk"
fi
"$TOOLS/emsdk/emsdk" install "$EMSDK_VERSION"
"$TOOLS/emsdk/emsdk" activate "$EMSDK_VERSION"
set +u
# shellcheck disable=SC1091
source "$TOOLS/emsdk/emsdk_env.sh"
set -u

cmake --version | head -n 1
doxygen --version
emcc --version | head -n 1

# Demo, API docs, site
cd "$ROOT/scripts"

bash demo-web.sh

emcmake cmake -S .. -B ../.docs-build -DBUILD_DOCS=ON
cmake --build ../.docs-build --target CoNLL-U-docs

bash pages.sh

for file in index.html conllu.js conllu.wasm demo.json api/index.html; do
    test -f "$ROOT/.pages/$file" || { echo "missing .pages/$file" >&2; exit 1; }
done
