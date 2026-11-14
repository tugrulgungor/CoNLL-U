#
# CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
#
# Copyright (c) 2026. All rights reserved.
# Tuğrul Güngör - https://conll-u.tugrulgungor.me
#
# Distributed under the MIT License.
# https://opensource.org/license/mit/
#

rm -r -f ../demo/.dist

emcmake cmake -S .. -B ../.emscripten-build -DBUILD_DEMO=ON
emmake cmake --build ../.emscripten-build --target CoNLL-U-demo

cp ../demo/demo.html ../demo/.dist/index.html
cp ../demo/demo.json ../demo/.dist/demo.json
cp ../demo/worker.js ../demo/.dist/worker.js

mkdir ../demo/.dist/data
cp -r ../tests/.data/**.conllu ../demo/.dist/data
