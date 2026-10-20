#!/bin/sh
# Cross-compiles the Fliqlo screen saver add-on on a workstation with Docker,
# into dist/<arch>/Fliqlo.
#
#   tools/cross-build.sh x86       gcc2, haiku/cross-compiler:x86_gcc2h-r1beta4
#   tools/cross-build.sh x86_64    haiku/cross-compiler:x86_64-r1beta4
#   tools/cross-build.sh arm64     container with a Haiku arm64 build tree; also
#                                  dist/arm64/support/{libscreensaver.so,ScreenSaver}
#   tools/cross-build.sh all
#
#   ARM64_CONTAINER   default haiku-builder
#   ARM64_GENERATED   default /root/renku-arm64-work/generated.arm64
#
# x86 is built with gcc2, not the hybrid's modern compiler: screen_blanker on
# an x86_gcc2 hybrid is a gcc2 program and only loads gcc2 add-ons.
set -e

HERE="$(cd "$(dirname "$0")/.." && pwd)"
FLAGS="-O2 -s -Wall -Wno-multichar -shared -o Fliqlo src/Fliqlo.cpp"
LIBS="-lbe -lscreensaver"

build()
{
	ARCH="$1"
	OUT="$HERE/dist/$ARCH"
	mkdir -p "$OUT"
	case "$ARCH" in
	x86)
		# The image's gcc2 looks for headers in its own sys-include, which
		# it does not have; the Haiku headers are under /system/develop.
		( cd "$HERE" && tar cf - src ) \
			| docker run --rm -i --platform linux/amd64 haiku/cross-compiler:x86_gcc2h-r1beta4 sh -c "
				set -e
				ln -s /system/develop/headers /tools/cross-tools-x86_gcc2/i586-pc-haiku/sys-include
				mkdir -p /work && cd /work && tar xf -
				/tools/cross-tools-x86_gcc2/bin/i586-pc-haiku-g++ $FLAGS \
					-B/system/develop/lib/ -L/system/develop/lib $LIBS >&2
				tar cf - Fliqlo" \
			| tar xf - -C "$OUT"
		;;
	x86_64)
		( cd "$HERE" && tar cf - src ) \
			| docker run --rm -i --platform linux/amd64 haiku/cross-compiler:x86_64-r1beta4 sh -c "
				set -e
				mkdir -p /work && cd /work && tar xf -
				x86_64-unknown-haiku-g++ -fPIC $FLAGS $LIBS >&2
				tar cf - Fliqlo" \
			| tar xf - -C "$OUT"
		;;
	arm64)
		CONTAINER="${ARM64_CONTAINER:-haiku-builder}"
		GEN="${ARM64_GENERATED:-/root/renku-arm64-work/generated.arm64}"
		P="$GEN/objects/haiku/arm64/packaging/packages_build/minimum"
		R="$GEN/objects/haiku/arm64/release"
		# RENKU arm64 images are @minimum builds: they have screen_blanker
		# and the input filter that starts it, but neither libscreensaver.so
		# nor the ScreenSaver preferences. Both come from the same tree
		# (jam -q ScreenSaver builds the preferences) and go in the package.
		docker exec "$CONTAINER" test -x "$R/preferences/screensaver/ScreenSaver" \
			|| { echo "build it first: cd $GEN && jam -q ScreenSaver" >&2; exit 1; }
		WORK="/tmp/fliqlo-cross-$$"
		( cd "$HERE" && tar cf - src ) \
			| docker exec -i "$CONTAINER" sh -c "
				set -e
				mkdir -p $WORK/sysroot/boot/system $WORK/lib && cd $WORK && tar xf -
				ln -sfn $P/hpkg_-haiku_devel.hpkg/contents/develop $WORK/sysroot/boot/system/develop
				SYSLIBS=\$(ls -d $GEN/build_packages/gcc_syslibs-*-arm64/lib | head -1)
				ln -sfn \$SYSLIBS/libstdc++.so.6 $WORK/lib/libstdc++.so
				ln -sfn \$SYSLIBS/libgcc_s.so.1 $WORK/lib/libgcc_s.so
				$GEN/cross-tools-arm64/bin/aarch64-unknown-haiku-g++ --sysroot=$WORK/sysroot \
					-fPIC -L$P/hpkg_-haiku.hpkg/contents/lib -L$R/kits/screensaver \
					-L$WORK/lib $FLAGS $LIBS >&2
				# What @minimum images leave out of the screen saver kit.
				mkdir -p support
				cp $R/kits/screensaver/libscreensaver.so \
					$R/preferences/screensaver/ScreenSaver support/
				tar cf - Fliqlo support
				rm -rf $WORK" \
			| tar xf - -C "$OUT"
		;;
	*)
		echo "usage: $0 x86|x86_64|arm64|all" >&2
		exit 1
		;;
	esac
	file "$OUT/Fliqlo"
}

if [ "$1" = all ]; then
	for a in x86 x86_64 arm64; do
		build $a
	done
else
	build "$1"
fi
