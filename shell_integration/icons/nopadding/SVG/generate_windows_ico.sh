#!/bin/sh

# Dimensions taken from https://www.apriorit.com/dev-blog/357-shell-extentions-basics-samples-common-problems#_Toc408244375
convert -background transparent attention.svg -gravity SouthWest \( -clone 0 -resize 10x10 -extent 16x16 \) \( -clone 0 -resize 16x16 -extent 32x32 \) \( -clone 0 -resize 24x24 -extent 48x48 \) \( -clone 0 -resize 128x128 -extent 256x256 \) -delete 0 ../../../windows/OCOverlays/ico/Warning.ico
convert -background transparent error.svg -gravity SouthWest \( -clone 0 -resize 10x10 -extent 16x16 \) \( -clone 0 -resize 16x16 -extent 32x32 \) \( -clone 0 -resize 24x24 -extent 48x48 \) \( -clone 0 -resize 128x128 -extent 256x256 \) -delete 0 ../../../windows/OCOverlays/ico/Error.ico
convert -background transparent ok.svg -gravity SouthWest \( -clone 0 -resize 10x10 -extent 16x16 \) \( -clone 0 -resize 16x16 -extent 32x32 \) \( -clone 0 -resize 24x24 -extent 48x48 \) \( -clone 0 -resize 128x128 -extent 256x256 \) -delete 0 ../../../windows/OCOverlays/ico/OK.ico
convert -background transparent shared.svg -gravity SouthWest \( -clone 0 -resize 10x10 -extent 16x16 \) \( -clone 0 -resize 16x16 -extent 32x32 \) \( -clone 0 -resize 24x24 -extent 48x48 \) \( -clone 0 -resize 128x128 -extent 256x256 \) -delete 0 ../../../windows/OCOverlays/ico/OK_Shared.ico
convert -background transparent sync.svg -gravity SouthWest \( -clone 0 -resize 10x10 -extent 16x16 \) \( -clone 0 -resize 16x16 -extent 32x32 \) \( -clone 0 -resize 24x24 -extent 48x48 \) \( -clone 0 -resize 128x128 -extent 256x256 \) -delete 0 ../../../windows/OCOverlays/ico/Sync.ico
