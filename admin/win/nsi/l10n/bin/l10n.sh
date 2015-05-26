#!/bin/bash
L10NDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
SCRIPTDIR="$L10NDIR/bin"
PODIR="$L10NDIR/pofiles"

# messages.pot will be used as English translation
cp -f $PODIR/messages.pot $PODIR/en.po

# generate all the languages nsh files
python $SCRIPTDIR/build_locale_nsi.py -o $L10NDIR -p $PODIR -l "English"

# for future references: the windows code pages
# 874 — Thai
# 932 — Japanese
# 936 — Chinese (simplified) (PRC, Singapore)
# 949 — Korean
# 950 — Chinese (traditional) (Taiwan, Hong Kong)
# 1200 — Unicode (BMP of ISO 10646, UTF-16LE)
# 1201 — Unicode (BMP of ISO 10646, UTF-16BE)
# 1250 — Latin (Central European languages)
# 1251 — Cyrillic
# 1252 — Latin (Western European languages, replacing Code page 850)
# 1253 — Greek
# 1254 — Turkish
# 1255 — Hebrew
# 1256 — Arabic
# 1257 — Latin (Baltic languages)
# 1258 — Vietnamese

# convert file to proper content
cd $L10NDIR
iconv -t CP1252 -o German.nsh German.nsh
iconv -t CP1252 -o Basque.nsh Basque.nsh
iconv -t CP1252 -o English.nsh English.nsh
iconv -t CP1252 -o Galician.nsh Galician.nsh
iconv -t CP1253 -o Greek.nsh Greek.nsh    
iconv -t CP1250 -o Slovenian.nsh Slovenian.nsh
iconv -t CP1257 -o Estonian.nsh Estonian.nsh
iconv -t CP1252 -o Italian.nsh Italian.nsh
iconv -t CP1252 -o PortugueseBR.nsh PortugueseBR.nsh
iconv -t CP1252 -o Spanish.nsh Spanish.nsh
iconv -t CP1252 -o Dutch.nsh Dutch.nsh
iconv -t CP1252 -o Finnish.nsh Finnish.nsh
iconv -t CP932 -o Japanese.nsh Japanese.nsh
iconv -t CP1250 -o Slovak.nsh Slovak.nsh
iconv -t CP1254 -o Turkish.nsh Turkish.nsh
iconv -t CP1252 -o Norwegian.nsh Norwegian.nsh


