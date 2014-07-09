#!/bin/sh

osascript -e 'tell application "Finder" \
                try \
                   «event NVTYload» \
                end try \
              end tell'

