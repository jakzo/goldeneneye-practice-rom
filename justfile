image := "goldeneye"

_default:
    just -l

# cleans up all files related to building the rom
clean:
    docker run --rm -v $(pwd):/home/dev {{ image }} make nuke

# cleans everything to start from a clean slate
clean_all:
    just clean || true
    docker rmi {{ image }}
    echo "Make sure to run 'just setup' again."

# run this to setup your local machine for development
setup:
    docker build -t {{ image }} .
    if [ -f baserom.u.z64 ]; then docker run --rm -v $(pwd):/home/dev {{ image }} ./scripts/extract_baserom.u.sh; fi
    if [ -f baserom.e.z64 ]; then docker run --rm -v $(pwd):/home/dev {{ image }} ./scripts/extract_diff.e.sh; fi
    if [ -f baserom.j.z64 ]; then docker run --rm -v $(pwd):/home/dev {{ image }} ./scripts/extract_diff.j.sh; fi

# build the rom
make:
    if test -z "$(docker images -q {{ image }})"; then just setup; fi
    docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j$(nproc) COMPARE=0 FINAL=YES
    if [ -f baserom.e.z64 ]; then docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j$(nproc) COMPARE=0 FINAL=YES VERSION=EU; fi
    if [ -f baserom.j.z64 ]; then docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j$(nproc) COMPARE=0 FINAL=YES VERSION=JP; fi

make-debug:
    docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j$(nproc) COMPARE=0 FINAL=NO
    if [ -f baserom.e.z64 ]; then docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j$(nproc) COMPARE=0 FINAL=NO VERSION=EU; fi
    if [ -f baserom.j.z64 ]; then docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j$(nproc) COMPARE=0 FINAL=NO VERSION=JP; fi

make-clean:
    git clean -fdx -e '.vscode/*' -e 'baserom.u.z64' -e '**/*.c' -e '**/*.h'
    docker image rm {{ image }}
    docker build -t {{ image }} .
    docker run --rm -v $(pwd):/home/dev {{ image }} ./scripts/extract_baserom.u.sh
    sleep 3
    docker run --rm -v $(pwd):/home/dev {{ image }} make -j8 COMPARE=0 FINAL=NO

# builds the rom and uploads it to a connected summercart64
sc64: make
    sc64deployer sd mkdir /CUSTOM || true
    sc64deployer sd upload build/u/ge007.u.z64 /CUSTOM/practice-rom_latest_ntsc_u.z64
    if [ -f build/e/ge007.e.z64 ]; then sc64deployer sd upload build/e/ge007.e.z64 /CUSTOM/practice-rom_latest_pal.z64; fi
    if [ -f build/j/ge007.j.z64 ]; then sc64deployer sd upload build/j/ge007.j.z64 /CUSTOM/practice-rom_latest_ntsc_j.z64; fi
