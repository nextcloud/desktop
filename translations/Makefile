#
# Translation
#--------------------------------------

.PHONY: l10n-push
l10n-push:
	tx -d push -s --skip --no-interactive

.PHONY: l10n-pull
l10n-pull:
	tx -d pull --skip --minimum-perc=75

.PHONY: l10n-clean
l10n-clean:
	rm -rf l10n/
	git checkout .tx/config

.PHONY: l10n-read
l10n-read:
	sudo chown -R user .
	cd .. && lupdate src -no-obsolete -ts translations/client_en.ts

.PHONY: l10n-write
l10n-write: ;
