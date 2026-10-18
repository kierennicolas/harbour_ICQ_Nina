#!/bin/bash
# Compiles (without linking) the app's Sailfish-only C++ sources against
# desktop Qt 5 and a stub sailfishapp.h.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
Q=$(qmake -query QT_INSTALL_HEADERS)
rc=0
for f in "$ROOT"/app/src/*.cpp "$ROOT"/tests/qmlcheck/main.cpp; do
  echo "compile ${f#$ROOT/}"
  g++ -std=c++11 -fPIC -Wall -c "$f" -o /dev/null -DAPP_VERSION='"ci"' \
      -I"$ROOT/tests/stubs" -I"$ROOT/app/src" -I"$ROOT/app/src/core" \
      -I"$ROOT/liboscar" -I"$ROOT/liboscar/tasks" -I"$ROOT/liboscar/utils" -I"$ROOT/liboscar/compat" \
      -I"$Q" -I"$Q/QtCore" -I"$Q/QtGui" -I"$Q/QtQml" -I"$Q/QtQuick" -I"$Q/QtDBus" -I"$Q/QtNetwork" -I"$Q/QtSql" \
      || rc=1
done
exit $rc
