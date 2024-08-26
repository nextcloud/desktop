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

.PHONY: python-lint
python-lint:
	black --check --diff .

.PHONY: python-lint-fix
python-lint-fix:
	black .

.PHONY: gherkin-lint
gherkin-lint:
	gherlint -c ./config/.gherlintrc.json .

.PHONY: gherkin-lint-fix
gherkin-lint-fix:
	gherlint --fix -c ./config/.gherlintrc.json .
