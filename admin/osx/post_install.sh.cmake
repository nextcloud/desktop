#!/bin/sh

# SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2015 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later

# Register and enable the Finder Sync extension.
#
# pluginkit manages the PlugInKit subsystem *per user*, but this postinstall script
# runs as root. Running pluginkit directly here mutates root's registry and is a no-op
# for the logged-in user — the extension then never appears/enables for them (issues
# #8471, #10032). So we hop into the console user's context (launchctl asuser + sudo -u)
# before calling pluginkit. Every step logs its result to /var/log/install.log so a
# failed registration is recognisable from installer logs users provide.
APPEX="/Applications/@APPLICATION_NAME@.app/Contents/PlugIns/FinderSyncExt.appex/"
EXT_ID="@APPLICATION_REV_DOMAIN@.FinderSyncExt"
LOG_PREFIX="Nextcloud FinderSync:"

CONSOLE_USER=$(stat -f%Su /dev/console 2>/dev/null)
CONSOLE_UID=$(id -u "$CONSOLE_USER" 2>/dev/null)

run_as_console_user() {
    launchctl asuser "$CONSOLE_UID" sudo -u "$CONSOLE_USER" "$@"
}

if [ -z "$CONSOLE_USER" ] || [ "$CONSOLE_USER" = "root" ] || [ -z "$CONSOLE_UID" ] || [ "$CONSOLE_UID" -eq 0 ]; then
    echo "$LOG_PREFIX no console user logged in; skipping pluginkit registration (extension will register on first launch of the app by the user)"
elif [ ! -x "$(command -v pluginkit)" ]; then
    echo "$LOG_PREFIX pluginkit not found; cannot register extension for user '$CONSOLE_USER'"
else
    echo "$LOG_PREFIX registering extension for user '$CONSOLE_USER' (uid $CONSOLE_UID)"

    # Add it to the DB. This happens automatically too, but we push a bit harder (#3463).
    run_as_console_user pluginkit -a "$APPEX"; rc=$?
    if [ "$rc" -eq 0 ]; then
        echo "$LOG_PREFIX pluginkit -a succeeded for $APPEX"
    else
        echo "$LOG_PREFIX WARNING pluginkit -a failed (rc $rc) for $APPEX"
    fi

    # Since El Capitan we need to wait for discovery before electing (#4650).
    sleep 10

    # Enable (elect) it for the console user.
    run_as_console_user pluginkit -e use -i "$EXT_ID"; rc=$?
    if [ "$rc" -eq 0 ]; then
        echo "$LOG_PREFIX pluginkit -e use succeeded for $EXT_ID"
    else
        echo "$LOG_PREFIX WARNING pluginkit -e use failed (rc $rc) for $EXT_ID"
    fi

    # Record the resulting election state for support diagnostics.
    if run_as_console_user pluginkit -m -i "$EXT_ID" -v; then
        echo "$LOG_PREFIX extension present in pluginkit database after registration (see line above for election state)"
    else
        echo "$LOG_PREFIX WARNING extension NOT present in pluginkit database after registration for $EXT_ID"
    fi
fi

# Remove legacy LaunchAgent plist from all users if present, became obsolete with version 33.0.0
dscl . -list /Users NFSHomeDirectory 2>/dev/null | awk '{print $2}' | while read -r USER_HOME; do
    LAUNCH_AGENT_PLIST="$USER_HOME/Library/LaunchAgents/@APPLICATION_REV_DOMAIN@.plist"

    if [ -f "$LAUNCH_AGENT_PLIST" ]; then
        rm "$LAUNCH_AGENT_PLIST"
    fi
done

exit 0
