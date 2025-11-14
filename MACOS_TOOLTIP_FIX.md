<!--
SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# macOS 15+ Tray Icon Tooltip Fix

## Problem
The tray icon hover message (tooltip) was not working on macOS 15+ (Sequoia).

## Root Cause
Apple changed how NSStatusItem tooltips work in macOS 15. Previously, tooltips could be set on the NSStatusItem itself, but macOS 15 requires them to be set on the `NSStatusItem.button` property.

Qt's `QSystemTrayIcon::setToolTip()` method was not updated to handle this change, causing tooltips to not display on macOS 15+.

## Solution
Implemented a native macOS workaround that:
1. Calls Qt's base implementation for backward compatibility
2. On macOS 15+, uses Objective-C runtime introspection to access Qt's internal NSStatusItem
3. Sets the tooltip directly on `NSStatusItem.button.toolTip`

## Files Modified
- `src/gui/systray.h` - Added declaration and override for setToolTip
- `src/gui/systray.cpp` - Implemented platform-specific setToolTip wrapper
- `src/gui/systray_mac_common.mm` - Implemented native macOS tooltip fix

## Testing Instructions

### Prerequisites
- macOS 15.0 (Sequoia) or later
- Nextcloud Desktop Client built with this fix

### Test Cases

#### Test 1: Basic Tooltip Display
1. Launch the Nextcloud Desktop Client
2. Wait for the tray icon to appear in the menu bar
3. Hover over the tray icon
4. **Expected**: A tooltip should appear showing the sync status
5. **Example**: "Nextcloud: Syncing 25MB (3 minutes left)"

#### Test 2: Tooltip Updates
1. Start a file sync operation
2. Hover over the tray icon periodically during sync
3. **Expected**: Tooltip should update to show current sync progress
4. After sync completes, tooltip should show success message

#### Test 3: Multiple Account Tooltips
1. Configure multiple Nextcloud accounts
2. Hover over the tray icon
3. **Expected**: Tooltip should show status for all accounts

#### Test 4: Error State Tooltips
1. Disconnect from network or server
2. Hover over the tray icon
3. **Expected**: Tooltip should show disconnection message
4. **Example**: "Disconnected from accounts: Account1: Network error"

#### Test 5: Backward Compatibility (macOS 14.x)
1. Build and test on macOS 14.x or earlier
2. Hover over the tray icon
3. **Expected**: Tooltip should work as before (no regression)

### Verification
- Check the Console.app for log messages:
  - Look for: "Successfully set tooltip on NSStatusItem.button for macOS 15+"
  - Or: "Could not access NSStatusItem directly, tooltip may not work on macOS 15+"

## Technical Details

### Implementation Approach
The fix uses Objective-C runtime APIs to access Qt's private NSStatusItem:
```objc
// Find the QCocoaSystemTrayIcon object in trayIcon's children
// Use class_copyIvarList to find the m_statusItem member
// Access it with object_getIvar and set tooltip on button
statusItem.button.toolTip = toolTip.toNSString();
```

### Why Runtime Introspection?
- Qt doesn't provide public API to access the native NSStatusItem
- The QCocoaSystemTrayIcon class is internal to Qt's platform plugin
- Runtime introspection is necessary to access the private member

### Safety Considerations
- The code is defensive and handles failures gracefully
- It falls back to Qt's standard implementation if introspection fails
- Memory is properly managed (ivars array is freed after use)
- Only applies the workaround on macOS 15+ (using @available check)

## Known Limitations
- Relies on Qt's internal implementation details (fragile)
- May need updates if Qt changes its internal structure
- Only applies to macOS 15+ (by design)

## Future Improvements
- Monitor Qt bug tracker for official fix
- Update to use official API when available
- Consider submitting patch to Qt project
