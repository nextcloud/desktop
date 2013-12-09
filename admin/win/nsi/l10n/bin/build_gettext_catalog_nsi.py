##############################################################################
#
#  PROJECT:     Multi Theft Auto v1.0
#  LICENSE:     See LICENSE in the top level directory
#  FILE:        utils/build_gettext_catalog_nsi.py
#  PURPOSE:     Create a template .pot file from a .NSI script
#  DEVELOPERS:  Dan Chowdhury <>
#
#  Multi Theft Auto is available from http://www.multitheftauto.com/
#
##############################################################################
import collections
import polib
import datetime
import os
from optparse import OptionParser

parser = OptionParser()
parser.add_option("-i", "--input", dest="input",
                  help="nightly.nsi location", default="../Shared/installer/nightly.nsi" )
parser.add_option("-o", "--output", dest="output",
                  help="POT file output location", default="installer.pot")
parser.add_option("-p", "--project", dest="project",
                  help="Project name to write to the POT file", default="MTA San Andreas Installer" )
parser.add_option("-v", "--version", dest="version",
                  help="Version to write to the POT file", default="1.x" )
parser.add_option("-l", "--lang", dest="lang",
                  help="Default language of the NSI", default="English" )

(options, args) = parser.parse_args()

metadata = {
    "Project-Id-Version" : (options.project + " " + options.version).strip(),
    "Report-Msgid-Bugs-To" : "",
    "POT-Creation-Date" : datetime.datetime.now().strftime('%Y-%m-%d %H:%M%z'),
    "PO-Revision-Date" : "YEAR-MO-DA HO:MI+ZONE",
    "Last-Translator" : "FULL NAME <EMAIL@ADDRESS>",
    "Language-Team" : "LANGUAGE <LL@li.org>",
    "Language"  : "",
    "MIME-Version" : "1.0",
    "Content-Type" : "text/plain; charset=UTF-8",
    "Content-Transfer-Encoding" : "8bit"
}

NSIFilePath = options.input

# Removes trailing \ which marks a new line
def removeEscapedNewLine(line):
    newline = line.rstrip("\n")
    newline = line.rstrip()
    newlen = len(newline);
    if newline.rfind("\\")+1 == len(newline):
        return newline[:newlen-1]
    return line

# Open our source file
NSIWorkingFile = open(NSIFilePath,"r")
NSIWorkingFileDir,NSIFileName = os.path.split(NSIFilePath)
# Create our new .POT file, and give our metadata
poFile = polib.POFile()
poFile.metadata = metadata
# Create a cache of messageValues : [ [fileName1,lineNumber1], [fileName2,lineNumber2]... ]  (The same message could appear on multiple lines)
LangStringCache = collections.OrderedDict()
# Create a cache of messageValues : [ label1, label2 ] (The same message could have multiple NSIS labels)
LangStringLabels = {}

# What we're doing here is looping through each line of our .nsi till we find a LangString of the default language
# Then, we try and grab the line number, the label, and the text
# The text can be multiline, so we have to sometimes continue reading till we reach the end
line=NSIWorkingFile.readline()
lineNo = 1
while line != '':
    commands =  line.split()
    if len(commands) > 3:
        if commands[0] == "LangString" and commands[2].upper() == ("${LANG_%s}"%options.lang).upper():
            label = commands[1]
            value = ""
            # Let's assume it's a one-liner
            start = line.find('"') + 1
            if start:
                end = line.find('"',start)
                if end != -1:
                    value = line[start:end]
                else: # Nope, multiline
                    line = removeEscapedNewLine(line)
                    # Keep reading till we reach the end
                    value = line[start:]
                    line = NSIWorkingFile.readline()
                    lineNo += 1
                    while line != '':
                        line = removeEscapedNewLine(line)
                        end = line.find('"')
                        if end != -1: #If we found the closing character, append
                            value += line[:end].lstrip()
                            break
                        else: #If not, append and continue
                            value += line.lstrip()
                        line=NSIWorkingFile.readline()
                        lineNo += 1

            # Remove whitespace and new lines
            value = value.strip("\t\n")
            value = polib.unescape ( value )
            if not value in LangStringCache:
                LangStringCache[value] = []
            # Note down our file and line number
            LangStringCache[value].append([options.input,lineNo])

            if not value in LangStringLabels:
                LangStringLabels[value] = []
            # Note down our label
            LangStringLabels[value].append(label)
            
    line=NSIWorkingFile.readline()
    lineNo += 1

# Now, we loop through our cache and build PO entries for each
# We use PO comment field to store our NSIS labels, so we can decode it back later
for msgid,lineOccurances in LangStringCache.iteritems():
    entry = polib.POEntry(
        msgid=msgid,
        msgstr='',
        occurrences=lineOccurances,
        comment=(" ").join(LangStringLabels[msgid])
    )
    poFile.append(entry)


NSIWorkingFile.close()

# Finally, let's generate our POT file
poFile.save(options.output)

print ( "NSI POT Export Operation complete" )
