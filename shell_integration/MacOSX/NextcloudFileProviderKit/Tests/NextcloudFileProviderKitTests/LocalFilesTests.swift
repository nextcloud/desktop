//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
import Testing

struct LocalFilesTests {
    @Test func recognisesLockFileNames() {
        // Microsoft Office.
        #expect(isLockFileName("~$Test.docx"))
        // LibreOffice.
        #expect(isLockFileName(".~lock.Test.odt#"))
        // Adobe InDesign / InCopy.
        #expect(isLockFileName("~Test~0kjyv(.idlk"))
        #expect(isLockFileName("Test.idlk"))
        #expect(isLockFileName("Test.IDLK")) // Case-insensitive extension.
        // Adobe Premiere Pro.
        #expect(isLockFileName("Test.prlock"))
        // AutoCAD.
        #expect(isLockFileName("Drawing.dwl"))
        #expect(isLockFileName("Drawing.dwl2"))
        #expect(isLockFileName("drawing.DWL")) // Case-insensitive extension.

        // Guarded documents themselves are not lock files.
        #expect(!isLockFileName("Test.indd"))
        #expect(!isLockFileName("Test.icml"))
        #expect(!isLockFileName("Test.prproj"))
        #expect(!isLockFileName("Test.dwg"))
        #expect(!isLockFileName("Test.docx"))
        #expect(!isLockFileName("Test.odt"))
    }

    @Test func recognisesAdobeLockFileNames() {
        #expect(isAdobeLockFileName("~Test~0kjyv(.idlk"))
        #expect(isAdobeLockFileName("Test.prlock"))
        #expect(isAdobeLockFileName("Test.IdLk")) // Case-insensitive extension.

        #expect(!isAdobeLockFileName("~$Test.docx")) // Microsoft Office is not Adobe.
        #expect(!isAdobeLockFileName(".~lock.Test.odt#")) // LibreOffice is not Adobe.
        #expect(!isAdobeLockFileName("Test.indd"))
        #expect(!isAdobeLockFileName("Test.prproj"))
        #expect(!isAdobeLockFileName("Drawing.dwl")) // AutoCAD is not Adobe.
    }

    @Test func recognisesAutoCADLockFileNames() {
        #expect(isAutoCADLockFileName("Drawing.dwl"))
        #expect(isAutoCADLockFileName("Drawing.dwl2"))
        #expect(isAutoCADLockFileName("drawing.DWL2")) // Case-insensitive extension.

        #expect(!isAutoCADLockFileName("~$Test.docx")) // Microsoft Office is not AutoCAD.
        #expect(!isAutoCADLockFileName(".~lock.Test.odt#")) // LibreOffice is not AutoCAD.
        #expect(!isAutoCADLockFileName("Test.idlk")) // Adobe is not AutoCAD.
        #expect(!isAutoCADLockFileName("Test.prlock"))
        #expect(!isAutoCADLockFileName("Test.dwg")) // The document itself is not a lock file.
    }

    @Test func extractsAdobeDocumentBaseName() {
        // InDesign / InCopy with a random token.
        #expect(adobeLockFileDocumentBaseName("~Test~0kjyv(.idlk") == "Test")
        // InDesign / InCopy without a token.
        #expect(adobeLockFileDocumentBaseName("~Test(.idlk") == "Test")
        // Base name containing a tilde is preserved (only the trailing token is dropped).
        #expect(adobeLockFileDocumentBaseName("~My~Doc~0kjyv(.idlk") == "My~Doc")
        // Premiere Pro.
        #expect(adobeLockFileDocumentBaseName("Test.prlock") == "Test")

        // Non-Adobe extensions yield no base name.
        #expect(adobeLockFileDocumentBaseName("Test.docx") == nil)
        #expect(adobeLockFileDocumentBaseName("~$Test.docx") == nil)
    }

    @Test func resolvesAutoCADLockFileTargetName() {
        // .dwl lock file resolves to .dwg document.
        #expect(autoCADLockFileTargetName("Drawing.dwl") == "Drawing.dwg")
        // .dwl2 lock file resolves to the same .dwg document.
        #expect(autoCADLockFileTargetName("Drawing.dwl2") == "Drawing.dwg")
        // Case-insensitive extension matching.
        #expect(autoCADLockFileTargetName("drawing.DWL") == "drawing.dwg")

        // Non-AutoCAD extensions yield no target name.
        #expect(autoCADLockFileTargetName("Test.docx") == nil)
        #expect(autoCADLockFileTargetName("Test.idlk") == nil)
        #expect(autoCADLockFileTargetName("Test.dwg") == nil) // The document is not a lock file.
    }

    @Test func recognisesAffinityLockFileNames() {
        #expect(isAffinityLockFileName("Screenshot.afphoto~lock~"))
        #expect(isAffinityLockFileName("Icon.afdesign~lock~"))
        #expect(isAffinityLockFileName("Layout.afpub~lock~"))
        #expect(isAffinityLockFileName("Screenshot.af~lock~"))
        #expect(isAffinityLockFileName("MY DOCUMENT.afdesign~lock~")) // Case-insensitive.

        #expect(!isAffinityLockFileName("~$Test.docx")) // Microsoft Office is not Affinity.
        #expect(!isAffinityLockFileName(".~lock.Test.odt#")) // LibreOffice is not Affinity.
        #expect(!isAffinityLockFileName("Test.idlk")) // Adobe is not Affinity.
        #expect(!isAffinityLockFileName("Test.prlock")) // Adobe is not Affinity.
        #expect(!isAffinityLockFileName("Drawing.dwl")) // AutoCAD is not Affinity.
        #expect(!isAffinityLockFileName("Screenshot.afphoto")) // The document itself is not a lock file.
    }

    @Test func resolvesAffinityLockFileTargetName() {
        #expect(affinityLockFileTargetName("Screenshot.afphoto~lock~") == "Screenshot.afphoto")
        #expect(affinityLockFileTargetName("Icon.afdesign~lock~") == "Icon.afdesign")
        #expect(affinityLockFileTargetName("Layout.afpub~lock~") == "Layout.afpub")
        #expect(affinityLockFileTargetName("Screenshot.af~lock~") == "Screenshot.af")

        // Non-Affinity names yield no target name.
        #expect(affinityLockFileTargetName("Test.docx") == nil)
        #expect(affinityLockFileTargetName("Test.idlk") == nil)
        #expect(affinityLockFileTargetName("Screenshot.afphoto") == nil) // No ~lock~ suffix.
        #expect(affinityLockFileTargetName("~lock~") == nil) // Bare suffix with no base name.
    }
}
