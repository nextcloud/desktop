##############################################################################
#
#  PROJECT:     ownCloud v1.0
#  LICENSE:     See LICENSE in the top level directory
#
##############################################################################
import collections
import os
import polib
from optparse import OptionParser

parser = OptionParser()
parser.add_option("-o", "--output", dest="output",
                  help="Directory for localized output", default="../Shared/installer/nightly_localized.nsi")
parser.add_option("-p", "--podir", dest="podir",
                  help="Directory containing PO files", default="../Shared/installer/locale/")
parser.add_option("-l", "--lang", dest="lang",
                  help="Default language of the NSI", default="English" )

(options, args) = parser.parse_args()


# Define a dict to convert locale names to language names
localeToName = {
    "af" : "Afrikaans",
    "sq" : "Albanian",
    "ar" : "Arabic",
    "hy" : "Armenian",
    "eu" : "Basque",
    "be" : "Belarusian",
    "bs" : "Bosnian",
    "br" : "Breton",
    "bg" : "Bulgarian",
    "ca" : "Catalan",
    "bem" : "Cibemba",
    "hr" : "Croatian",
    "cs" : "Czech",
    "da" : "Danish",
    "nl" : "Dutch",
    "efi" : "Efik",
    "en" : "English",
    "eo" : "Esperanto",
    "et" : "Estonian",
    "fa" : "Farsi",
    "fi" : "Finnish",
    "fr" : "French",
    "gl" : "Galician",
    "ka" : "Georgian",
    "de" : "German",
    "el" : "Greek",
    "he" : "Hebrew",
    "hi" : "Hindi",
    "hu" : "Hungarian",
    "is" : "Icelandic",
    "ig" : "Igbo",
    "id" : "Indonesian",
    "ga" : "Irish",
    "it" : "Italian",
    "ja" : "Japanese",
    "km" : "Khmer",
    "ko" : "Korean",
    "ku" : "Kurdish",
    "lv" : "Latvian",
    "lt" : "Lithuanian",
    "lb" : "Luxembourgish",
    "mk" : "Macedonian",
    "mg" : "Malagasy",
    "ms" : "Malay",
    "mn" : "Mongolian",
    "nb_NO" : "Norwegian",
    "nn" : "NorwegianNynorsk",
    "ps" : "Pashto",
    "pl" : "Polish",
    "pt" : "Portuguese",
    "pt_BR" : "PortugueseBR",
    "ro" : "Romanian",
    "ru" : "Russian",
    "sr" : "Serbian",
    "sr_sp" : "SerbianLatin",
    "st" : "Sesotho",
    "sn" : "Shona",
    "zh_CN" : "SimpChinese",
    "sk" : "Slovak",
    "sl" : "Slovenian",
    "es" : "Spanish",
    "es_AR" : "SpanishInternational",
    "sw" : "Swahili",
    "sv" : "Swedish",
    "ta" : "Tamil",
    "th" : "Thai",
    "zh_HK" : "TradChinese",
    "tr" : "Turkish",
    "tw" : "Twi",
    "uk" : "Ukrainian",
    "ug" : "Uyghur",
    "uz" : "Uzbek",
    "ca@valencia" : "Valencian",
    "vi" : "Vietnamese",
    "cy" : "Welsh",
    "yo" : "Yoruba",
    "zu" : "Zulu",
}

def escapeNSIS(st):
    return st.replace('\\', r'$\\')\
             .replace('\t', r'$\t')\
             .replace('\r', r'\r')\
             .replace('\n', r'\n')\
             .replace('\"', r'$\"')\
             .replace('$$\\', '$\\')

translationCache = {}

# The purpose of this loop is to go to the podir scanning for PO files for each locale name
# Once we've found a PO file, we use PO lib to read every translated entry
# Using this, for each each language, we store a dict of entries - { nsilabel (comment) : translation (msgstr) }
# For untranslated entries, we use msgid instead of msgstr (i.e. default English string)
for root,dirs,files in os.walk(options.podir):
    for file in files:
        filename,ext = os.path.splitext(file)
        if ext == ".po":
            # Valid locale filename (fr.po, de.po etc)?
            if filename in localeToName:
                language = localeToName[filename]
                translationCache[language] = collections.OrderedDict()
                
                po = polib.pofile(os.path.join(root,file))
                for entry in po.translated_entries():
                    # Loop through all our labels and add translation (each translation may have multiple labels)
                    for label in entry.comment.split():
                        translationCache[language][label] = escapeNSIS(entry.msgstr)
                # For untranslated strings, let's add the English entry
                for entry in po.untranslated_entries():
                    for label in entry.comment.split():
                        print("Warning: Label '%s' for language '%s' remains untranslated"%(label,language))
                        translationCache[language][label] = escapeNSIS(entry.msgid)

def tostr(obj):
    if type(obj) == unicode:
        return obj.encode("utf-8")
    else:
        return obj

NSILanguages = []
NSIDeclarations = []

# file header
NSILanguages.append( tostr('; Auto-generated - do not modify\n') )
NSIDeclarations.append( tostr('; Auto-generated - do not modify\n') )

# loopthrough the languages an generate one nsh files for each language
lineNo = 1
for language,translations in translationCache.iteritems():
    NSINewLines = []
    NSINewLines.append( tostr('# Auto-generated - do not modify\n') )
    count = 0
    # if the language isn't the default, we add our MUI_LANGUAGE macro
    if language.upper() != options.lang.upper():
        NSILanguages.append( tostr('!insertmacro MUI_LANGUAGE "%s"\n'%language) )

    # For every translation we grabbed from the .po, let's add our StrCpy command
    for label,value in translations.iteritems():
        NSINewLines.append( tostr('StrCpy $%s "%s"\n' % (label,value)) )
        if language.upper() == options.lang.upper():
	    NSIDeclarations.append( tostr('Var %s\n' % label) )

        count += 1
    NSIWorkingFile = open('%s/%s.nsh' % (options.output, language),"w")
    NSIWorkingFile.writelines(NSINewLines)
    NSIWorkingFile.close()
    print ( "%i translations merged for language '%s'"%(count,language) )
    
# Finally, let's write languages.nsh and declarations.nsh
NSIWorkingFile = open('%s/languages.nsh' % options.output,"w")
NSIWorkingFile.writelines(NSILanguages)
NSIWorkingFile.close()

NSIWorkingFile = open('%s/declarations.nsh' % options.output,"w")
NSIWorkingFile.writelines(NSIDeclarations)
NSIWorkingFile.close()
    
print ( "NSI Localization Operation Complete" )
