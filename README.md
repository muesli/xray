xray
====

xray compares media files by their perceptual hash and identifies dupes.

## Dependencies
xray depends on QtCore (tested with Qt5, should work with Qt4) and phash.
You should be able to find existing packages for your system.
On Ubuntu install "libphash0-dev", on Arch install "phash" from AUR.

## Build it
    qmake xray.pro
    make

## Try it out
    ./xray /media/videos
