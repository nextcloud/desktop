<!--
  - SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# macOS App Sandbox Support for Qt Applications

## Overview

This document explains how to make the Nextcloud Desktop Client work properly with macOS App Sandbox when using Qt. The key issue is that Qt's `QFileDialog` returns security-scoped URLs that require explicit access management in sandboxed applications.

## The Problem

When running a sandboxed macOS application with the `com.apple.security.files.user-selected.read-write` entitlement, file operations on user-selected files (via `QFileDialog`) will fail unless you explicitly:

1. Call `startAccessingSecurityScopedResource()` on the URL before accessing the file
2. Call `stopAccessingSecurityScopedResource()` when done

This is **required by macOS sandbox security**, but Qt does not handle this automatically. The underlying issue is that:

- `QFileDialog::getSaveFileUrl()` returns a `QUrl` that represents a security-scoped bookmark
- Without calling `startAccessingSecurityScopedResource()`, the sandboxed app has no permission to access the file
- Even though you have the entitlement, you must explicitly claim access for each user-selected file

## The Solution

### 1. Security-Scoped Access Wrapper

We created a RAII wrapper class `MacSandboxSecurityScopedAccess` (in `utility_mac_sandbox.h/mm`) that:

- Automatically calls `startAccessingSecurityScopedResource()` in the constructor
- Automatically calls `stopAccessingSecurityScopedResource()` in the destructor
- Uses unique_ptr for exception safety
- Provides `isValid()` to check if access was successfully obtained

### 2. Usage Pattern

```cpp
#ifdef Q_OS_MACOS
#include "utility_mac_sandbox.h"
#endif

void MyClass::saveFile()
{
    const auto fileUrl = QFileDialog::getSaveFileUrl(
        this,
        tr("Save File"),
        QUrl::fromLocalFile(QDir::homePath()),
        tr("Text Files (*.txt)")
    );

    if (fileUrl.isEmpty()) {
        return;
    }

#ifdef Q_OS_MACOS
    // Acquire security-scoped access for the user-selected file
    auto scopedAccess = Utility::MacSandboxSecurityScopedAccess::create(fileUrl);
    
    if (!scopedAccess->isValid()) {
        // Handle error - access could not be obtained
        QMessageBox::critical(this, tr("Error"), tr("Could not access file"));
        return;
    }
    // scopedAccess will automatically release when it goes out of scope
#endif

    // Now you can safely access the file
    QFile file(fileUrl.toLocalFile());
    if (file.open(QIODevice::WriteOnly)) {
        // Write to file...
    }
}
```

### 3. Required Entitlements

In `admin/osx/macosx.entitlements.cmake`, ensure you have:

```xml
<key>com.apple.security.app-sandbox</key>
<true/>
<key>com.apple.security.files.user-selected.read-write</key>
<true/>
```

## Key Requirements for Qt + macOS Sandbox

### 1. Use QFileDialog URL-based Methods

Always use the URL-based variants of QFileDialog methods:
- ✅ `QFileDialog::getSaveFileUrl()`
- ✅ `QFileDialog::getOpenFileUrl()`
- ✅ `QFileDialog::getOpenFileUrls()`
- ❌ `QFileDialog::getSaveFileName()` - returns QString, not security-scoped
- ❌ `QFileDialog::getOpenFileName()` - returns QString, not security-scoped

### 2. Wrap File Access with Security Scoping

```cpp
#ifdef Q_OS_MACOS
auto scopedAccess = Utility::MacSandboxSecurityScopedAccess::create(fileUrl);
if (!scopedAccess->isValid()) {
    // Handle error
    return;
}
#endif
// Access file here
// scopedAccess releases automatically when going out of scope
```

### 3. Handle Scope Lifetime Correctly

The security-scoped access must remain valid for the entire duration of file access:

```cpp
// ✅ CORRECT - scopedAccess lives until after file operations
#ifdef Q_OS_MACOS
auto scopedAccess = Utility::MacSandboxSecurityScopedAccess::create(fileUrl);
if (!scopedAccess->isValid()) {
    return;
}
#endif

QFile file(fileUrl.toLocalFile());
file.open(QIODevice::WriteOnly);
file.write(data);
file.close();
// scopedAccess destructor called here

// ❌ WRONG - scopedAccess destroyed before file operations
#ifdef Q_OS_MACOS
{
    auto scopedAccess = Utility::MacSandboxSecurityScopedAccess::create(fileUrl);
    if (!scopedAccess->isValid()) {
        return;
    }
} // scopedAccess destroyed here!
#endif

QFile file(fileUrl.toLocalFile());  // This will fail!
file.open(QIODevice::WriteOnly);    // No longer have access
```

### 4. Consider All File Operations

This applies to ANY file operation on user-selected files:
- Reading files
- Writing files
- Creating archives/zip files
- Copying files
- Moving files
- Checking file existence/permissions

## Common Pitfalls

### 1. Using QString-based paths instead of QUrl

```cpp
// ❌ WRONG - loses security-scoped bookmark
QString path = QFileDialog::getSaveFileName(...);

// ✅ CORRECT - preserves security-scoped bookmark
QUrl url = QFileDialog::getSaveFileUrl(...);
```

### 2. Converting QUrl too early

```cpp
// ❌ WRONG - converts to string before starting access
QUrl url = QFileDialog::getSaveFileUrl(...);
QString path = url.toLocalFile();  // Loses security scope!
#ifdef Q_OS_MACOS
auto access = Utility::MacSandboxSecurityScopedAccess::create(QUrl::fromLocalFile(path));  // Won't work
#endif

// ✅ CORRECT - start access before conversion
QUrl url = QFileDialog::getSaveFileUrl(...);
#ifdef Q_OS_MACOS
auto access = Utility::MacSandboxSecurityScopedAccess::create(url);  // Works!
#endif
QString path = url.toLocalFile();
```

### 3. Forgetting to check isValid()

```cpp
// ❌ RISKY - doesn't check if access was obtained
auto scopedAccess = Utility::MacSandboxSecurityScopedAccess::create(fileUrl);
QFile file(fileUrl.toLocalFile());  // Might fail silently

// ✅ CORRECT - always check validity
auto scopedAccess = Utility::MacSandboxSecurityScopedAccess::create(fileUrl);
if (!scopedAccess->isValid()) {
    // Show error to user
    return;
}
QFile file(fileUrl.toLocalFile());  // Now safe to use
```

## Testing Sandbox Behavior

To test if your app properly handles sandbox restrictions:

1. **Build with proper entitlements**: Ensure the app is codesigned with the entitlements file
2. **Test file operations**: Try to save/open files in various locations
3. **Check Console.app**: Look for sandbox violation messages like:
   ```
   Sandbox: MyApp(12345) deny(1) file-write-create /Users/...
   ```
4. **Test without access calls**: Temporarily remove the security-scoped access calls to verify they're needed

## References

- [Apple Documentation: App Sandbox](https://developer.apple.com/documentation/security/app_sandbox)
- [Apple Documentation: Security-Scoped Bookmarks](https://developer.apple.com/documentation/foundation/nsurl/1417051-startaccessingsecurityscopedreso)
- [Qt Documentation: QFileDialog](https://doc.qt.io/qt-6/qfiledialog.html)

## Files Modified

- `src/common/utility_mac_sandbox.h` - Header for security-scoped access wrapper
- `src/common/utility_mac_sandbox.mm` - Implementation using Objective-C++
- `src/common/common.cmake` - Added new files to build system
- `src/gui/generalsettings.cpp` - Fixed debug archive creation to use security-scoped access

## Future Work

Consider auditing all uses of `QFileDialog` in the codebase to ensure they:
1. Use URL-based methods (`getSaveFileUrl`, `getOpenFileUrl`, etc.)
2. Properly acquire security-scoped access on macOS
3. Handle access errors gracefully
