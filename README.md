# webm-player

This is a simple player for WebM video files. Currently it plays the
video only (no audio).

## Dependencies

You will need the following third party libraries to build:

- [nestegg](https://github.com/kinetiknz/nestegg) (retrieved in this repo as a submodule)
- [libvpx](https://chromium.googlesource.com/webm/libvpx) (retrieved in this repo as a submodule)
- libogg
- libvorbis

## Building

	git clone https://github.com/Keyaku/webm-player
	cd webm-player
	git submodule init
	git submodule update
	make
