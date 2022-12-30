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
