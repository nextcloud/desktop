from helpers.FilesHelper import get_file_size_on_disk, get_file_size


@Then('the placeholder of file "|any|" should exist on the file system')
def step(context, file_name):
    resource_path = getResourcePath(file_name)
    size_on_disk = get_file_size_on_disk(resource_path)
    test.compare(
        size_on_disk, 0, f"Size of the placeholder on the disk is: '{size_on_disk}'"
    )


@Then('the file "|any|" should be downloaded')
def step(context, file_name):
    resource_path = getResourcePath(file_name)
    size_on_disk = get_file_size_on_disk(resource_path)
    file_size = get_file_size(resource_path)
    test.compare(
        size_on_disk,
        file_size,
        f"Original file size '{file_size}' is not equal to its size on disk '{size_on_disk}'",
    )
