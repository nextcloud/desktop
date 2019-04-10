#
# Translation
#--------------------------------------

.PHONY: l10n-push
l10n-push:
	tx -d push -s --skip --no-interactive

.PHONY: l10n-pull
l10n-pull:
	tx -d pull -r owncloud.client-nsis --skip

.PHONY: l10n-clean
l10n-clean:
	rm -rf l10n/
	git checkout .tx/config

.PHONY: l10n-read
l10n-read: ;

.PHONY: l10n-write
l10n-write:
	pip install polib
	bin/l10n.sh
