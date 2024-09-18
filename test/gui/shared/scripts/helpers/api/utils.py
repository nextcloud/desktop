from posixpath import join


def url_join(*args):
    paths = []
    for path in list(args):
        paths.append(path.strip("/"))
    return join("", *paths)
