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
    [ -f baserom.u.z64 ] && docker run --rm -v $(pwd):/home/dev {{ image }} ./scripts/extract_baserom.u.sh
    [ -f baserom.e.z64 ] && docker run --rm -v $(pwd):/home/dev {{ image }} ./scripts/extract_diff.e.sh

# build the rom
make:
    if test -z "$(docker images -q {{ image }})"; then just setup; fi
    docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j$(nproc) COMPARE=0 FINAL=YES
    [ -f baserom.e.z64 ] && docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j$(nproc) COMPARE=0 FINAL=YES VERSION=EU

make-debug:
    docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j$(nproc) COMPARE=0 FINAL=NO
    [ -f baserom.e.z64 ] && docker run --rm -v "$(pwd):/home/dev" {{ image }} make -j$(nproc) COMPARE=0 FINAL=NO VERSION=EU

# builds the rom and uploads it to a connected summercart64
sc64: make
    sc64deployer sd mkdir /CUSTOM || true
    sc64deployer sd upload build/u/ge007.u.z64 /CUSTOM/practice-rom_latest_ntsc_u.z64
    sc64deployer sd upload build/u/ge007.u.z64 /CUSTOM/practice-rom_latest_pal.z64
