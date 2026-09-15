#
# CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
#
# Copyright (c) 2026. All rights reserved.
# Tuğrul Güngör - https://conll-u.tugrulgungor.me
#
# Distributed under the MIT License.
# https://opensource.org/license/mit/
#

rm -r -f ../.pages
mkdir ../.pages
cd ../.pages

cp -r ../demo/.dist/** ./

mkdir api
cp -r ../docs/html/** ./api
