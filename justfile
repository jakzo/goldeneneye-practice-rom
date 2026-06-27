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

# Full clean and re-extract of assets
reset:
    git clean -fdx -e '.vscode' -e 'baserom.u.z64' -e '**/*.c' -e '**/*.h'
    docker image rm {{ image }}
    docker build -t {{ image }} .
    docker run --rm -v "$(pwd):/home/dev" {{ image }} ./scripts/extract_baserom.u.sh

# run this to setup your local machine for development
setup:
    docker build -t {{ image }} .
    docker run --rm -v "$(pwd):/home/dev" {{ image }} ./scripts/extract_baserom.u.sh
    if [ -f baserom.e.z64 ]; then docker run --rm -v $(pwd):/home/dev {{ image }} ./scripts/extract_diff.e.sh; fi
    if [ -f baserom.j.z64 ]; then docker run --rm -v $(pwd):/home/dev {{ image }} ./scripts/extract_diff.j.sh; fi

# build the rom
make:
    if test -z "$(docker images -q {{ image }})"; then just setup; fi
    docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j{{ num_cpus() }}
    if [ -f baserom.e.z64 ]; then docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j{{ num_cpus() }} VERSION=EU; fi
    if [ -f baserom.j.z64 ]; then docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j{{ num_cpus() }} VERSION=JP; fi

make-dev:
    docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j{{ num_cpus() }} DEV=1
    if [ -f baserom.e.z64 ]; then docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j{{ num_cpus() }} DEV=1 VERSION=EU; fi
    if [ -f baserom.j.z64 ]; then docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j{{ num_cpus() }} DEV=1 VERSION=JP; fi

make-clean: reset
    docker run --rm -v $(pwd):/home/dev {{ image }} make -j{{ num_cpus() }}

ares:
    docker run --rm -v $(pwd):/home/dev {{ image }} make -j{{ num_cpus() }}
    ares ./build/u/ge007.u.z64

ares-dev BOOT_LEVEL="TITLE":
    docker run --rm -v $(pwd):/home/dev {{ image }} make -j{{ num_cpus() }} DEV=1 BOOT_LEVEL={{ BOOT_LEVEL }}
    ares ./build/u/ge007.u.z64

test TEST_CASE="STATE_DOOR":
    docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j{{ num_cpus() }} DEV=1 TEST_CASE="{{ TEST_CASE }}"
    ares ./build/u/ge007.u.z64

test-all:
    ./scripts/run_practice_tests.py

sc64-dev BOOT_LEVEL="TITLE":
    docker run --rm -v $(pwd):/home/dev {{ image }} make -j{{ num_cpus() }} {{ BOOT_LEVEL }}
    sc64deployer upload build/u/ge007.u.z64
    sc64deployer debug

# builds the rom and uploads it to a connected summercart64
sc64: make
    sc64deployer sd mkdir /CUSTOM || true
    sc64deployer sd upload build/u/ge007.u.z64 /CUSTOM/practice-rom_latest_ntsc_u.z64
    if [ -f build/e/ge007.e.z64 ]; then sc64deployer sd upload build/e/ge007.e.z64 /CUSTOM/practice-rom_latest_pal.z64; fi
    if [ -f build/j/ge007.j.z64 ]; then sc64deployer sd upload build/j/ge007.j.z64 /CUSTOM/practice-rom_latest_ntsc_j.z64; fi
