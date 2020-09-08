# Desktop client documentation

- The main nextcloud Documentation is found at https://github.com/nextcloud/documentation
- The rst files from the current stable branch will be parsed with sphinx to be used at https://docs.nextcloud.com/desktop/3.0/

## How to build the documentation

- After installing [sphinx](https://www.sphinx-doc.org) you can run:

```
$ git clone https://github.com/nextcloud/desktop.git
$ cd desktop
$ cd docs
$ sphinx-build -b html -D html_theme='nextcloud_com' -d _build/doctrees   . _build/html/com
```

The documentation html files will be at ```desktop/docs/_build/html/com```.
