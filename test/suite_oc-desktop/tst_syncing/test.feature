Feature: Syncing files

    As a user
    I want to be able to sync my local folders to to my owncloud server
    so that I dont have to upload and download files manually

    Scenario: Syncing a file to the server
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
            [ownCloud]
            remotePollInterval=5000
            """
        When the user creates a file "lorem-for-upload.txt" with following content on the file system
            """
            test content
            """
        And the user waits for file "lorem-for-upload.txt" to get synced
        Then as "Alice" the file "lorem-for-upload.txt" on the server should have the content "test content"

    Scenario: Syncing a file from the server
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
            [ownCloud]
            remotePollInterval=5000
            """
        And user "Alice" has uploaded file on the server with content "test content" to "uploaded-lorem.txt"
        When the user waits for file "uploaded-lorem.txt" to get synced
        Then the file "uploaded-lorem.txt" should exist on the file system with following content
            """
            test content
            """

    Scenario: Syncing a file from the server and creating conflict
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
            [ownCloud]
            remotePollInterval=5000
            """
        And user "Alice" has uploaded file on the server with content "test content" to "uploaded-lorem.txt"
        And the user has waited for file "conflict.txt" to get synced
        And the user has paused the file sync
        And the user has changed the content of local file "conflict.txt" to:
            """
            client content
            """
        And user "Alice" has uploaded file on the server with content "server content" to "uploaded-lorem.txt"
        When the user resumes the file sync on the client
        And the user clicks on the activity tab
        And user selects the unsynced files tab with 1 unsynced files
        # Then an conflict warning should be shown for 1 files
        Then the table for conflict warning should include file "conflict.txt"
        And the file "conflict.txt" should exist on the file system with following content
            """
            server content
            """
        And a conflict file for "conflict.txt" should exist on the file system with following content
            """
            client content
            """