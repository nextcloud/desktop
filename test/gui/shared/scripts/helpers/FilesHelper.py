import os


def buildConflictedRegex(filename):
    if '.' in filename:
        # TODO: improve this for complex filenames
        namepart = filename.split('.')[0]
        extpart = filename.split('.')[1]
        return '%s \(conflicted copy \d{4}-\d{2}-\d{2} \d{6}\)\.%s' % (
            namepart,
            extpart,
        )
    else:
        return '%s \(conflicted copy \d{4}-\d{2}-\d{2} \d{6}\)' % (filename)


def sanitizePath(path):
    return path.replace('//', '/')


def can_read(resource):
    can_read = False
    try:
        f = open(resource)
        f.close()
        can_read = True
    except:
        pass
    return can_read and os.access(resource, os.R_OK)


def can_write(resource):
    can_write = False
    try:
        f = open(resource, 'w')
        f.close()
        can_write = True
    except:
        pass
    return can_write and os.access(resource, os.W_OK)


def read_file_content(file):
    f = open(file, "r")
    content = f.read()
    f.close()
    return content
