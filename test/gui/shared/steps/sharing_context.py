import os

from pageObjects.PublicLinkDialog import PublicLinkDialog
from pageObjects.SharingDialog import SharingDialog

from helpers.SetupClientHelper import getResourcePath, substituteInLineCodes
from helpers.FilesHelper import sanitizePath
from helpers.SyncHelper import (
    getSocketConnection,
    waitForFileOrFolderToHaveSyncStatus,
    SYNC_STATUS,
)
from helpers.ConfigHelper import get_config


def shareResourceCommand(resource):
    socketConnect = getSocketConnection()
    socketConnect.sendCommand("SHARE:" + resource + "\n")
    if not socketConnect.read_socket_data_with_timeout(0.1):
        return False
    for line in socketConnect.get_available_responses():
        if line.startswith('SHARE:OK') and line.endswith(resource):
            return True
        elif line.endswith(resource):
            return False


def openSharingDialog(resource):
    resource = getResourcePath(resource)
    resourceExist = waitFor(
        lambda: os.path.exists(resource), get_config('maxSyncTimeout') * 1000
    )
    if not resourceExist:
        raise Exception("{} doesn't exists".format(resource))
    shareDialog = waitFor(
        lambda: shareResourceCommand(resource),
        get_config('maxSyncTimeout') * 1000,
    )
    if not shareDialog:
        raise Exception("Sharing dialog didn't open for {}".format(resource))


def createPublicLinkShare(
    resource, password='', permissions='', expireDate='', name=''
):
    openSharingDialog(resource)
    PublicLinkDialog.openPublicLinkTab()
    PublicLinkDialog.createPublicLink(password, permissions, expireDate, name)


def createPublicShareWithRole(resource, role):
    resource = sanitizePath(substituteInLineCodes(resource))
    openSharingDialog(resource)
    PublicLinkDialog.openPublicLinkTab()
    PublicLinkDialog.createPublicLinkWithRole(role)


def collaboratorShouldBeListed(receiver, resource, permissions, receiverCount=0):
    # wait for client to the view
    waitForFileOrFolderToHaveSyncStatus(
        getResourcePath(), "FOLDER", SYNC_STATUS["UPDATE"]
    )
    openSharingDialog(resource)

    checkCollaboratorAndPremissions(receiver, permissions, receiverCount)

    SharingDialog.closeSharingDialog()


def checkCollaboratorAndPremissions(receiver, permissions, receiverCount=0):
    permissionsList = permissions.split(',')

    # findAllObjects: This function finds and returns a list of object references identified by the symbolic or real (multi-property) name objectName.
    sharedWithObj = SharingDialog.getCollaborators()

    #     we use sharedWithObj list from above while verifying if users are listed or not.
    #     For this we need an index value i.e receiverCount
    #     For 1st user in the list the index will be 0 which is receiverCount default value
    #     For 2nd user in the list the index will be 1 and so on

    test.compare(str(sharedWithObj[receiverCount].text), receiver)
    test.compare(
        SharingDialog.hasEditPermission(),
        ('edit' in permissionsList),
    )
    test.compare(
        SharingDialog.hasSharePermission(),
        ('share' in permissionsList),
    )


@When(
    'the user adds "|any|" as collaborator of resource "|any|" with permissions "|any|" using the client-UI'
)
def step(context, receiver, resource, permissions):
    openSharingDialog(resource)
    SharingDialog.addCollaborator(receiver, permissions)
    SharingDialog.closeSharingDialog()


@When('the user adds following collaborators of resource "|any|" using the client-UI')
def step(context, resource):
    openSharingDialog(resource)

    # In the following loop we are trying to share resource with given permission to one user at a time given from the data table in the feature file
    for count, row in enumerate(context.table[1:]):
        receiver = row[0]
        permissions = row[1]
        SharingDialog.addCollaborator(receiver, permissions, False, count + 1)

    SharingDialog.closeSharingDialog()


@When(
    'the user selects "|any|" as collaborator of resource "|any|" using the client-UI'
)
def step(context, receiver, resource):
    openSharingDialog(resource)
    SharingDialog.selectCollaborator(receiver)


@When(
    'the user adds group "|any|" as collaborator of resource "|any|" with permissions "|any|" using the client-UI'
)
def step(context, receiver, resource, permissions):
    openSharingDialog(resource)
    SharingDialog.addCollaborator(receiver, permissions, True)
    SharingDialog.closeSharingDialog()


@Then(
    'user "|any|" should be listed in the collaborators list for file "|any|" with permissions "|any|" on the client-UI'
)
def step(context, receiver, resource, permissions):
    collaboratorShouldBeListed(receiver, resource, permissions)


@Then(
    'group "|any|" should be listed in the collaborators list for file "|any|" with permissions "|any|" on the client-UI'
)
def step(context, receiver, resource, permissions):
    receiver += " (group)"
    collaboratorShouldBeListed(receiver, resource, permissions)


@When('the user opens the public links dialog of "|any|" using the client-UI')
def step(context, resource):
    openSharingDialog(resource)
    PublicLinkDialog.openPublicLinkTab()


@When("the user toggles the password protection using the client-UI")
def step(context):
    PublicLinkDialog.togglePassword()


@Then('the password progress indicator should not be visible in the client-UI')
def step(context):
    waitFor(lambda: (test.vp("publicLinkPasswordProgressIndicatorInvisible")))


@Then(
    'the password progress indicator should not be visible in the client-UI - expected to fail'
)
def step(context):
    waitFor(lambda: (test.xvp("publicLinkPasswordProgressIndicatorInvisible")))


@When('the user opens the sharing dialog of "|any|" using the client-UI')
def step(context, resource):
    openSharingDialog(resource)


@Then('the text "|any|" should be displayed in the sharing dialog')
def step(context, fileShareContext):
    test.compare(
        SharingDialog.getSharingDialogMessage(),
        fileShareContext,
    )


@Then('the error text "|any|" should be displayed in the sharing dialog')
def step(context, fileShareContext):
    test.compare(
        SharingDialog.getSharingDialogMessage(),
        fileShareContext,
    )


@When(
    'the user creates a new public link for file "|any|" without password using the client-UI'
)
def step(context, resource):
    createPublicLinkShare(resource)


@When(
    'the user creates a new public link for file "|any|" with password "|any|" using the client-UI'
)
def step(context, resource, password):
    createPublicLinkShare(resource, password)


@Then('the expiration date of the last public link of file "|any|" should be "|any|"')
def step(context, resource, expiryDate):
    # wait for client to the view
    waitForFileOrFolderToHaveSyncStatus(
        getResourcePath(), "FOLDER", SYNC_STATUS["UPDATE"]
    )
    openSharingDialog(resource)
    PublicLinkDialog.openPublicLinkTab()

    if expiryDate.strip("%") == "default":
        expiryDate = PublicLinkDialog.getDefaultExpiryDate()
    actualExpiryDate = PublicLinkDialog.getExpirationDate()
    test.compare(expiryDate, actualExpiryDate)

    SharingDialog.closeSharingDialog()


@When('the user edits the public link named "|any|" of file "|any|" changing following')
def step(context, publicLinkName, resource):
    expireDate = ''
    for row in context.table:
        if row[0] == 'expireDate':
            expireDate = row[1]
            break
    PublicLinkDialog.setExpirationDate(expireDate)


@When(
    'the user creates a new public link with permissions "|any|" for folder "|any|" without password using the client-UI'
)
def step(context, permissions, resource):
    createPublicLinkShare(resource, '', permissions)


@When(
    'the user creates a new public link with permissions "|any|" for folder "|any|" with password "|any|" using the client-UI'
)
def step(context, permissions, resource, password):
    createPublicLinkShare(resource, password, permissions)


@When('the user creates a new public link with following settings using the client-UI:')
def step(context):
    linkSettings = {}
    for row in context.table:
        linkSettings[row[0]] = row[1]

    if "path" not in linkSettings:
        raise Exception("'path' is required but not given.")

    if "expireDate" in linkSettings and linkSettings['expireDate'] == "%default%":
        linkSettings['expireDate'] = linkSettings['expireDate'].strip("%")

    createPublicLinkShare(
        resource=linkSettings['path'],
        password=linkSettings['password'] if "password" in linkSettings else None,
        expireDate=linkSettings['expireDate'] if "expireDate" in linkSettings else None,
    )


@When(
    'the user creates a new public link for folder "|any|" using the client-UI with these details:'
)
def step(context, resource):
    role = ''
    for row in context.table:
        if row[0] == 'role':
            role = row[1]
            break

    if role == '':
        raise Exception("No role has been found")
    else:
        createPublicShareWithRole(resource, role)


@When(
    'the user removes permissions "|any|" for user "|any|" of resource "|any|" using the client-UI'
)
def step(context, permissions, receiver, resource):
    openSharingDialog(resource)
    SharingDialog.removePermissions(permissions)


@Step("the user closes the sharing dialog")
def step(context):
    SharingDialog.closeSharingDialog()


@Then(
    '"|any|" permissions should not be displayed for user "|any|" for resource "|any|" on the client-UI'
)
def step(context, permissions, user, resource):
    permissionsList = permissions.split(',')

    editChecked, shareChecked = SharingDialog.getAvailablePermission()

    if 'edit' in permissionsList:
        test.compare(editChecked, False)

    if 'share' in permissionsList:
        test.compare(shareChecked, False)


@Then('the error "|any|" should be displayed in the sharing dialog')
def step(context, errorMessage):
    test.compare(SharingDialog.getErrorText(), errorMessage)


@When(
    'the user tires to share resource "|any|" with the group "|any|" using the client-UI'
)
def step(context, resource, group):
    openSharingDialog(resource)

    SharingDialog.selectCollaborator(group, True)


@When(
    'the user unshares the resource "|any|" for collaborator "|any|" using the client-UI'
)
def step(context, resource, receiver):
    openSharingDialog(resource)
    SharingDialog.unshareWith(receiver)


@When('the user deletes the public link for file "|any|"')
def step(context, resource):
    openSharingDialog(resource)
    PublicLinkDialog.openPublicLinkTab()
    PublicLinkDialog.deletePublicLink()


@When(
    'the user changes the password of public link "|any|" to "|any|" using the client-UI'
)
def step(context, publicLinkName, password):
    PublicLinkDialog.changePassword(password)


@Then(
    'the following users should be listed in as collaborators for file "|any|" on the client-UI'
)
def step(context, resource):
    # wait for client to the view
    waitForFileOrFolderToHaveSyncStatus(
        getResourcePath(), "FOLDER", SYNC_STATUS["UPDATE"]
    )
    openSharingDialog(resource)
    #     Here we are trying to verify if the user added in when step are listed in the client-UI or not
    #     We now have a variable name receiverCount which is used in collaboratorShouldBeListed function call
    receiverCount = 0
    for row in context.table[1:]:
        receiver = row[0]
        permissions = row[1]

        checkCollaboratorAndPremissions(receiver, permissions, receiverCount)
        receiverCount += 1

    SharingDialog.closeSharingDialog()


@When('the user searches for collaborator "|any|" using the client-UI')
def step(context, collaborator):
    SharingDialog.searchCollaborator(collaborator)


@When(
    'the user searches for collaborator with autocomplete characters "|any|" using the client-UI'
)
def step(context, collaborator):
    SharingDialog.searchCollaborator(collaborator)


@Then('the following users should be listed as suggested collaborators:')
def step(context):
    for collaborator in context.table[1:]:
        test.compare(
            SharingDialog.isUserInSuggestionList(collaborator[0]),
            True,
            "Assert user '" + collaborator[0] + "' is listed",
        )


@Then('the collaborators should be listed in the following order:')
def step(context):
    for index, collaborator in enumerate(context.table[1:], start=1):
        test.compare(
            SharingDialog.getCollaboratorName(index),
            collaborator[0],
        )
