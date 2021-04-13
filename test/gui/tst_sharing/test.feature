Feature: Sharing

    As a user
    I want to share files and folders with other users
    So that those users can access the files and folders

    Scenario: simple sharing
        Given user "Alice" has been created on the server with default attributes
        And user "Brian" has been created on the server with default attributes
        And user "Alice" has set up a client with default settings and password "1234"
        When the user adds "Brian Murphy" as collaborator of resource "%client_sync_path%/textfile0.txt" with permissions "edit,share" using the client-UI
        Then user "Brian Murphy" should be listed in the collaborators list for file "%client_sync_path%/textfile0.txt" with permissions "edit,share" on the client-UI

    @issue-7459
    Scenario: Progress indicator should not be visible after unselecting the password protection checkbox while sharing through public link
        Given user "Alice" has been created on the server with default attributes
        And user "Alice" has set up a client with default settings and password "1234"
        When the user opens the public links dialog of "%client_sync_path%/textfile0.txt" using the client-UI
        And the user toggles the password protection using the client-UI
        And the user toggles the password protection using the client-UI
        Then the progress indicator should not be visible in the client-UI

    @issue-7423
    Scenario: unshare a reshared file
        Given the setting "shareapi_auto_accept_share" on the server of app "core" has been set to "no"
        And the administrator on the server has set the default folder for received shares to "Shares"
        And user "Alice" has been created on the server with default attributes
        And user "Brian" has been created on the server with default attributes
        And user "Carol" has been created on the server with default attributes
        And user "Alice" has shared folder "simple-folder" on the server with user "Brian"
        And user "Brian" has accepted the share "simple-folder" on the server offered by user "Alice"
        And user "Brian" has shared folder "Shares/simple-folder" on the server with user "Carol"
        And user "Brian" has set up a client with these settings and password "AaBb2Cc3Dd4":
        """
        [ownCloud]
        remotePollInterval=5000
        """
        And user "Alice" has updated the share permissions on the server for folder "/simple-folder" to "read" for user "Brian"
        When user "Brian" opens the sharing dialog of "%client_sync_path%/Shares/simple-folder" using the client-UI
        Then the error text "The file can not be shared because it was shared without sharing permission." should be displayed in the sharing dialog


    Scenario: simple sharing of a file by public link without password
        Given user "Alice" has been created on the server with default attributes
        And user "Alice" has set up a client with default settings and password "1234"
        When the user creates a new public link for file "%client_sync_path%/textfile0.txt" without password using the client-UI
        Then as user "Alice" the file "textfile0.txt" should have a public link on the server
        And the public should be able to download the file "textfile0.txt" without password from the last created public link by "Alice" on the server


    Scenario: simple sharing of a file by public link with password
        Given user "Alice" has been created on the server with default attributes
        And user "Alice" has set up a client with default settings and password "1234"
        When the user creates a new public link for file "%client_sync_path%/textfile0.txt" with password "pass123" using the client-UI
        Then as user "Alice" the file "textfile0.txt" should have a public link on the server
        And the public should be able to download the file "textfile0.txt" with password "pass123" from the last created public link by "Alice" on the server


    Scenario: user changes the expiration date of an already existing public link using webUI
        Given user "Alice" has been created on the server with default attributes
        And user "Alice" has set up a client with default settings and password "1234"
        And user "Alice" has created a public link on the server with following settings
            | path       | textfile0.txt |
            | name       | Public link   |
            | expireDate | 2038-10-14    |
        When the user opens the public links dialog of "%client_sync_path%/textfile0.txt" using the client-UI
        And the user edits the public link named "Public link" of file "textfile0.txt" changing following
            | expireDate | 2038-07-21 |
        Then the fields of the last public link share response of user "Alice" should include on the server
            | expireDate | 2038-07-21 |


	Scenario: simple sharing of a folder by public link without password
        Given user "Alice" has been created on the server with default attributes
        And user "Alice" has set up a client with these settings and password "1234":
            """
            [Accounts]
            0\Folders\1\ignoreHiddenFiles=true
            0\Folders\1\localPath=%client_sync_path%
            0\Folders\1\paused=false
            0\Folders\1\targetPath=/
            0\Folders\1\version=2
            0\Folders\1\virtualFilesMode=off
            0\dav_user=alice
            0\display-name=Alice
            0\http_oauth=false
            0\http_user=alice
            0\url=%local_server%
            0\user=Alice
            0\version=1
            version=2
            """
        When the user creates a new public link with permissions "Download / View" for folder "%client_sync_path%/simple-folder" without password using the client-UI
        Then as user "Alice" the folder "simple-folder" should have a public link on the server
        And the public should be able to download the folder "lorem.txt" without password from the last created public link by "Alice" on the server


	Scenario: simple sharing of a folder by public link with password
        Given user "Alice" has been created on the server with default attributes
        And user "Alice" has set up a client with these settings and password "1234":
            """
            [Accounts]
            0\Folders\1\ignoreHiddenFiles=true
            0\Folders\1\localPath=%client_sync_path%
            0\Folders\1\paused=false
            0\Folders\1\targetPath=/
            0\Folders\1\version=2
            0\Folders\1\virtualFilesMode=off
            0\dav_user=alice
            0\display-name=Alice
            0\http_oauth=false
            0\http_user=alice
            0\url=%local_server%
            0\user=Alice
            0\version=1
            version=2
            """
        When the user creates a new public link with permissions "Download / View " for folder "%client_sync_path%/simple-folder" with password "pass123" using the client-UI
        Then as user "Alice" the folder "simple-folder" should have a public link on the server
        And the public should be able to download the folder "lorem.txt" with password "pass123" from the last created public link by "Alice" on the server
