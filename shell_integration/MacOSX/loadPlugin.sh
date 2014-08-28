#!/bin/sh

osascript -e 'tell application "Finder" \
                try \
                   «event OWNCload» \
                end try \
              end tell'

