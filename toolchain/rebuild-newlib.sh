#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh
. $DIR/util.sh

pushd tarballs > /dev/null
    rm -rf "newlib-1.19.0"
    deco "newlib" "newlib-1.19.0.tar.gz"
    patc "newlib" "newlib-1.19.0"
    installNewlibStuff "newlib-1.19.0"
popd > /dev/null

pushd build
    if [ ! -d newlib ]; then
        mkdir newlib
    else
        rm -r newlib
        mkdir newlib
    fi
    pushd $DIR/tarballs/newlib-1.19.0/newlib/libc/sys
        autoconf || bail
        pushd toaru
            autoreconf || bail
            yasm -f elf -o crt0.o crt0.s || bail
            yasm -f elf -o crti.o crti.s || bail
            yasm -f elf -o crtn.o crtn.s || bail
            cp crt0.o ../
            cp crt0.o /tmp/__toaru_crt0.o
            cp crti.o ../
            cp crti.o /tmp/__toaru_crti.o
            cp crtn.o ../
            cp crtn.o /tmp/__toaru_crtn.o
        popd
    popd
    pushd newlib
        mkdir -p $TARGET/newlib/libc/sys
        cp /tmp/__toaru_crt0.o $TARGET/newlib/libc/sys/crt0.o
        rm /tmp/__toaru_crt0.o
        cp /tmp/__toaru_crti.o $TARGET/newlib/libc/sys/crti.o
        rm /tmp/__toaru_crti.o
        cp /tmp/__toaru_crtn.o $TARGET/newlib/libc/sys/crtn.o
        rm /tmp/__toaru_crtn.o
        echo "" > $DIR/tarballs/newlib-1.19.0/newlib/libc/stdlib/malign.c
        $DIR/tarballs/newlib-1.19.0/configure --target=$TARGET --prefix=$PREFIX || bail
        make || bail
        make install || bail
        cp -r $DIR/patches/newlib/include/* $PREFIX/$TARGET/include/
        cp $TARGET/newlib/libc/sys/crt*.o $PREFIX/$TARGET/lib/
    popd
popd
