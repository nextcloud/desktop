# Makefile for the documentation

SHELL ?= bash

FONTS_DIR ?= fonts
STYLES_DIR ?= resources/themes

STYLE ?= owncloud
REVDATE ?= "$(shell date +'%B %d, %Y')"

ifndef VERSION
	ifneq ($(DRONE_TAG),)
		VERSION ?= $(subst v,,$(DRONE_TAG))
	else
		ifneq ($(DRONE_BRANCH),)
			VERSION ?= $(subst /,,$(DRONE_BRANCH))
		else
			VERSION ?= master
		endif
	endif
endif

ifndef OUTPUT
	ifneq ($(VERSION),master)
		OUTPUT ?= build/desktop/$(VERSION)
	else
		OUTPUT ?= build/desktop
	endif
endif

#
# Remove build artifacts from output dir.
#
.PHONY: clean
clean:
	-rm -rf build/

#
# Generate PDF version of the manual.
#
.PHONY: pdf
pdf:
	asciidoctor-pdf \
		-a pdf-stylesdir=$(STYLES_DIR)/ \
		-a pdf-style=$(STYLE) \
		-a pdf-fontsdir=$(FONTS_DIR) \
		-a examplesdir=modules/ROOT/examples \
		-a imagesdir=modules/ROOT/assets/images \
		-a revnumber=$(VERSION) \
		-a revdate=$(REVDATE) \
		--base-dir $(CURDIR) \
		--out-file $(OUTPUT)/ownCloud_Desktop_Client_Manual.pdf \
		books/ownCloud_Desktop_Client_Manual.adoc
