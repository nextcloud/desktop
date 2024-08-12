# Main Makefile for formatting python code for gui test
#
# Requirements to run make here:
# - black
#
# Installing black can be done via https://black.readthedocs.io/en/stable/getting_started.html
# configuration for black can be found inside pyproject.toml file under [tools.black] section.
#
# Usage:
# make test-python-style
#	-auto format all the python files inside `test/gui`
# make test-python-style-check
#	-check which files will be reformatted
# make test-python-style-diff
#	-see what changes will be done
#

.PHONY: install
install: pnpm-install pnpm-install-chromium pip-install

.PHONY: pnpm-install
pnpm-install:
	cd webUI && pnpm install

.PHONY: pnpm-install-chromium
pnpm-install-chromium:
	cd webUI && pnpm exec playwright install chromium

.PHONY: pip-install
pip-install:
	python3.10 -m pip install -r requirements.txt

.PHONY: test-python-style
test-python-style:
	black .

.PHONY: test-python-style-diff
test-python-style-diff:
	black --diff .

.PHONY: test-python-style-check
test-python-style-check:
	black --check .

.PHONY: test-gherkin-lint
test-gherkin-lint:
	gherlint . -c ./config/.gherlintrc.json

.PHONY: test-gherkin-lint-fix
test-gherkin-lint-fix:
	gherlint --fix . -c ./config/.gherlintrc.json
