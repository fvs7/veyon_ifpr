#!/usr/bin/env bash
#
# Veyon IFPR (REA) - native Windows build inside MSYS2 (UCRT64)
# Copyright (c) 2026 IFPR - Campus União da Vitória - GNU GPL v2 or later
#
# Replaces the upstream cross-compilation (.ci/windows/build.sh), which depends on a
# private MinGW-w64 container image. Builds Veyon with the MSYS2 toolchain/packages,
# stages the same file layout as cmake/modules/WindowsInstaller.cmake, bundles every
# runtime DLL found via ntldd and runs makensis.
#
# Must be run from the source root inside an MSYS2 UCRT64 shell. Requires the
# Interception library (interception.h, libinterception.dll.a, interception.dll)
# to be installed into $MINGW_PREFIX beforehand (see the GitHub workflow).
#
# Output: veyon-<version>-win64-setup.exe in the source root.

set -euo pipefail

SRC=$(pwd)
BUILD=${BUILD_DIR:-$SRC/build-msys2}
PREFIX=${MINGW_PREFIX:-/ucrt64}
QT_PLUGINS=$PREFIX/share/qt6/plugins
STAGE=$BUILD/stage

echo "== Source: $SRC  Build: $BUILD  Prefix: $PREFIX"

# same source cleanup as upstream (removes MSVC project files etc. from UltraVNC)
"$SRC/.ci/common/strip-ultravnc-sources.sh" || true
cd "$SRC"

cmake -S "$SRC" -B "$BUILD" -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DWITH_LTO=OFF \
	-DWITH_WERROR=OFF \
	-DQT_TRANSLATIONS_DIR="$PREFIX/share/qt6/translations" \
	${CMAKE_FLAGS:-}

cmake --build "$BUILD" --parallel "$(nproc)"

echo "== Staging installer files"
cd "$BUILD"
rm -rf "$STAGE"
mkdir -p "$STAGE"/{interception,plugins,translations,crypto,imageformats,platforms,styles,tls}

cp "$SRC"/3rdparty/interception/* "$STAGE/interception/"
cp "$SRC/3rdparty/ddengine/ddengine64.dll" "$STAGE/"
cp core/veyon-core.dll "$STAGE/"
find . -mindepth 2 -name 'veyon-*.exe' -not -path './stage/*' -exec cp '{}' "$STAGE/" ';'
find plugins/ -name '*.dll' -exec cp '{}' "$STAGE/plugins/" ';'
# helper libraries built below plugins/ belong next to the executables (as upstream does)
for f in "$STAGE"/plugins/lib*.dll "$STAGE"/plugins/vnchooks.dll; do
	[ -e "$f" ] && mv "$f" "$STAGE/"
done
cp translations/*.qm "$STAGE/translations/"

cp "$PREFIX/bin/interception.dll" "$STAGE/"
cp "$QT_PLUGINS/crypto/libqca-ossl.dll" "$STAGE/crypto/"
cp "$QT_PLUGINS/imageformats/qjpeg.dll" "$STAGE/imageformats/"
cp "$QT_PLUGINS/platforms/qwindows.dll" "$STAGE/platforms/"
cp "$QT_PLUGINS"/styles/*.dll "$STAGE/styles/"
cp "$QT_PLUGINS/tls/qopensslbackend.dll" "$STAGE/tls/"

strip "$STAGE"/veyon-*.exe "$STAGE/veyon-core.dll" "$STAGE"/plugins/*.dll

echo "== Collecting runtime DLLs from $PREFIX/bin"
WIN_PREFIX_BIN=$(cygpath -m "$PREFIX/bin")
# repeat until no new DLL shows up (newly copied DLLs may pull in further ones)
while true; do
	added=0
	mapfile -t deps < <(
		find "$STAGE" -type f \( -iname '*.exe' -o -iname '*.dll' \) -not -path "$STAGE/interception/*" -print0 |
			xargs -0 ntldd -R 2>/dev/null |
			tr '\\' '/' |
			grep -io "${WIN_PREFIX_BIN}/[^ ]*\.dll" |
			sort -fu || true
	)
	for dep in "${deps[@]}"; do
		name=$(basename "$dep")
		if [ ! -e "$STAGE/$name" ]; then
			cp "$(cygpath -u "$dep")" "$STAGE/"
			echo "   + $name"
			added=1
		fi
	done
	[ $added -eq 0 ] && break
done

echo "== Checking for unresolved DLLs"
if find "$STAGE" -maxdepth 1 -type f \( -iname '*.exe' -o -iname '*.dll' \) -print0 |
	xargs -0 ntldd 2>/dev/null | grep -i "not found" | grep -iv "api-ms-win\|ext-ms-" ; then
	echo "WARNING: some DLLs could not be resolved (see above)"
fi

cp "$SRC/COPYING" "$STAGE/"
sed 's/$/\r/' "$SRC/COPYING" > "$STAGE/LICENSE.TXT"
sed 's/$/\r/' "$SRC/README.md" > "$STAGE/README.TXT"
cp -r "$SRC/nsis" "$STAGE/"
cp "$BUILD/nsis/veyon.nsi" "$STAGE/"
# the script is written for Linux makensis; the native Windows makensis only
# accepts backslashes as path separators in File and the MUI bitmaps/icons
sed -i -E -e '/^\s*File /s,/,\\,g' -e 's,"nsis/,"nsis\\,g' "$STAGE/veyon.nsi"

echo "== Staged files"
find "$STAGE" -type f | sed "s|$STAGE/||" | sort

echo "== Creating installer"
cd "$STAGE"
makensis -V3 veyon.nsi
mv veyon-*setup.exe "$SRC/"
ls -la "$SRC"/veyon-*setup.exe
