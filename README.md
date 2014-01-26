xray
====

xray compares media files by their perceptual hash and identifies dupes.

This means files are not compared byte-for-byte, but by their visual content.
It will (or at least it should) find duplicates of videos, even when they are
encoded in different formats, bitrates and/or resolutions.

## What it does
xray scans a directory. Each video file it finds gets analyzed by taking several
snapshots of the video content. Those images are then p-hashed and compared with
all other videos it found. If xray considers two videos similar enough it will
output that it found a dupe and additionally provides a similarity score.

In case xray thinks it found a perfect match, it will also calculate the sha1sum
of both videos to safely identify exact copies of a file.

## Dependencies
xray depends on QtCore (tested with Qt5, should work with Qt4), ffmpeg and phash.
You should be able to find existing packages for your system. On Ubuntu install
"libphash0-dev", on Arch install "phash" from AUR.

## Build it
    qmake xray.pro
    make

## Try it out
    ./xray /media/videos

## Plans
So far this is just a proof-of-concept. It works, but I have plans to enhance xray:
- Compare video frames and when finding a reasonably close one, do a close inspection comparing more of the surrounding frames of both files.
- CLI params for consts, like thresholds, hamming distance etc.
- Center-crop and re-scale frames to eliminate issues with borders and aspect ratios.
- Combine individual frame hashes into one big file hash.
- Store hashes in a file (sqlite/unqlite/textfile) to speed up future comparisons.
- Drop external snapshot process. Directly use libvlc or ffmpeg or such.

Enjoy!
