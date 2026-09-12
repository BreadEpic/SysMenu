export GITHASH 		:= $(shell git rev-parse --short HEAD)
export VERSION 		:= 2.2.0
export API_VERSION 	:= 6
export WANT_FLAC 	:= 1
export WANT_MP3 	:= 1
export WANT_WAV 	:= 1

all: overlay nxExt module

ZIP_NAME := SysMenu-$(VERSION)-$(GITHASH).zip

clean:
	$(MAKE) -C sys-tune/nxExt clean
	$(MAKE) -C overlay clean
	$(MAKE) -C sys-tune clean
	-rm -rf dist
	-rm -f SysMenu-*.zip sys-tune-*-*.zip

overlay:
	$(MAKE) -C overlay

nxExt:
	$(MAKE) -C sys-tune/nxExt

module:
	$(MAKE) -C sys-tune

# Builds the SD card layout in dist/ and zips it for release. Extracting the
# zip over the root of an SD card is all an end user has to do.
#
# The zip is made from inside dist/ so the trees land at the root of the
# archive, and is written to the parent so re-running never packs the previous
# zip into the new one. Note the && rather than ; when entering dist: with ; the
# recipe's exit status is that of the trailing cd, so a missing zip binary
# passes silently and ships a release with no archive in it.
dist: all
	@command -v zip >/dev/null 2>&1 || { echo "error: 'zip' is not installed. Install it with: pacman -S zip"; exit 1; }
	rm -rf dist $(ZIP_NAME)
	mkdir -p dist/switch/.overlays
	mkdir -p dist/atmosphere/contents/4200000000000000/flags
	touch dist/atmosphere/contents/4200000000000000/flags/boot2.flag
	cp sys-tune/sys-tune.nsp dist/atmosphere/contents/4200000000000000/exefs.nsp
	cp overlay/sys-tune-overlay.ovl dist/switch/.overlays/
	cp sys-tune/toolbox.json dist/atmosphere/contents/4200000000000000/
	cd dist && zip -r -q ../$(ZIP_NAME) atmosphere switch
	@echo built ... $(ZIP_NAME)
	-@command -v hactool >/dev/null 2>&1 && hactool -t nso sys-tune/sys-tune.nso || true

.PHONY: all overlay module dist clean
