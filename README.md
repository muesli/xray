xray
====

xray compares media files by their perceptual hash and identifies dupes.

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
- Take snapshots of videos in (e.g.) 5 second intervals.
- Compare video frames and when finding a reasonably close one, only then do a close inspection and compare more surrounding frames of both files.
- CLI params for consts, like thresholds, hamming distance etc.
- Center-crop frames to eliminate issues with borders.
- Combine frame hashes into one big file hash.
- Store hashes in a file (sqlite/unqlite/textfile) to speed up future comparisons.
- Drop external snapshot process. Maybe directly use libvlc or ffmpeg or such.

Enjoy!
