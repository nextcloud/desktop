# Desktop client documentation

The main nextcloud Documentation is found at https://github.com/nextcloud/documentation

The rst files from the current stable branch will be parsed with sphinx to be used at https://docs.nextcloud.com/desktop/latest/

## Dependencies

You will need to have [Sphinx](https://www.sphinx-doc.org), which comes packaged with Python 3.

In addition, run the following to install PdfLatex and Doxygen.

- On Linux:
```
$ sudo apt install doxygen python texlive-latex-base texlive-latex-extra
```
> Note: You may use something other than `apt` depending on your distribution.

- On macOS (via [Homebrew](https://brew.sh/)):
```
% brew install basictex doxygen python
```

## How to build the documentation

In your repositories directory:

```
$ git clone https://github.com/nextcloud/desktop.git
$ cd desktop
$ cd doc
$ sphinx-build -b html -D html_theme='nextcloud_com' -d _build/doctrees   . _build/html/com
```

The documentation html files will be at ```desktop/docs/_build/html/com```.
