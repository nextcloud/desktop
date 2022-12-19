#
# Translation
#--------------------------------------
# pull all changes push none, intended for local testing
.PHONY: dry
dry:  l10n-remove-old l10n-read l10n-pull

# remove outdated translations
.PHONY: l10n-remove-old
l10n-remove-old:
	rm *.ts
	git checkout client_en.ts

# update the generated source translation
.PHONY: l10n-read
l10n-read:
	cd .. && lupdate src -no-obsolete -ts translations/client_en.ts

# push the changed from l10n-read
.PHONY: l10n-push
l10n-push:
	tx push -s --skip --no-interactive

# pull all translation from transifex
.PHONY: l10n-pull
l10n-pull:
	tx pull --force --skip --all

# tx might apply changes to .tx/config, we don't want that
.PHONY: l10n-clean
l10n-clean:
	git checkout .tx/config
