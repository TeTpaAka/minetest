#!/bin/bash -e

set_linux_compiler_env() {
	if [[ "${COMPILER}" == "gcc-5.1" ]]; then
		export CC=gcc-5.1
		export CXX=g++-5.1
	elif [[ "${COMPILER}" == "gcc-6" ]]; then
		export CC=gcc-6
		export CXX=g++-6
	elif [[ "${COMPILER}" == "gcc-7" ]]; then
		export CC=gcc-7
		export CXX=g++-7
	elif [[ "${COMPILER}" == "clang-3.6" ]]; then
		export CC=clang-3.6
		export CXX=clang++-3.6
	elif [[ "${COMPILER}" == "clang-4.0" ]]; then
		export CC=clang-4.0
		export CXX=clang++-4.0
	fi
}

# Linux build only
install_linux_deps() {
	sudo apt-get update
	sudo apt-get install libirrlicht-dev cmake libbz2-dev libpng12-dev \
		libjpeg-dev libxxf86vm-dev libgl1-mesa-dev libsqlite3-dev \
		libhiredis-dev libogg-dev libgmp-dev libvorbis-dev libopenal-dev \
		gettext libpq-dev libleveldb-dev
}

# Mac OSX build only
install_macosx_deps() {
	brew update
	brew install freetype gettext hiredis irrlicht jpeg leveldb libogg libvorbis luajit
	#brew upgrade postgresql
}

# Relative to git-repository root:
TRIGGER_COMPILE_PATHS="src/.*\.(c|cpp|h)|CMakeLists.txt|cmake/Modules/|util/travis/|util/buildbot/"

needs_compile() {
	git diff --name-only $TRAVIS_COMMIT_RANGE | egrep -q "^($TRIGGER_COMPILE_PATHS)"
}

