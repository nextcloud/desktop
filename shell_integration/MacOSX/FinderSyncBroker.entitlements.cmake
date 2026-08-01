<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<!--
		Deliberately just these two keys.

		The broker vends a Mach service whose name is its own bundle identifier, which is
		prefixed by the app group below. The App Sandbox grants both mach-register and
		mach-lookup for any global name under a claimed app group prefix, so no
		com.apple.security.temporary-exception.* entitlement is needed here — nor in the app
		or the FinderSync extension. That is what keeps the whole bundle eligible for the
		Mac App Store, where temporary exceptions are prohibited.
	-->
	<key>com.apple.security.app-sandbox</key>
	<true/>
	<key>com.apple.security.application-groups</key>
	<array>
		<string>@DEVELOPMENT_TEAM@.@APPLICATION_REV_DOMAIN@</string>
	</array>
@DEBUG_ENTITLEMENTS@
</dict>
</plist>
