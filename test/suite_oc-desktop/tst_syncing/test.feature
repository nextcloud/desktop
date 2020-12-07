Feature: Syncing files

    As a user
    I want to be able to sync my local folders to to my owncloud server
    so that I dont have to upload and download files manually

    Scenario: Syncing a file to the server
        Given user 'Alice' has been created with default attributes
        And user 'Alice' has set up a client with these settings:
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
        When the user creates a file 'lorem.txt' with following content on the file system
            """
            test content
            """
        Then the file 'lorem.txt' should exist on the server for user 'Alice' after syncing with following content
            """
            test content
            """

    Scenario: Syncing a file from the server
        Given user 'Alice' has been created with default attributes
        And user 'Alice' has set up a client with these settings:
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
        When the user 'Alice' has uploaded a file 'lorem.txt' with following content
            """
            test content
            """
        Then the file 'lorem.txt' should exist on the file system after syncing with following content
            """
            test content
            """

    Scenario: Syncing a file from the server and creating conflict
        Given user 'Alice' has been created with default attributes
        And user 'Alice' has set up a client with these settings:
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
        When the user 'Alice' has uploaded a file 'lorem.txt' with following content
            """
            test content
            """
        Then the file 'lorem.txt' should exist on the file system after syncing with following content
            """
            test content
            """
        When the user pauses the file sync on the client
        And the user changes the content of local file 'lorem.txt' with following content
            """
            client content
            """
        And the user 'Alice' updates the file 'lorem.txt' on the server to
            """
            server content
            """
        And the user resumes the file sync on the client
        And the user clicks on the activity tab
        And user selects the unsynced files tab with 1 unsynced files
        # Then an conflict warning should be shown for 1 files
        Then the table for conflict warning should include file 'lorem.txt'
        And the file 'lorem.txt' should exist on the file system with following content
            """
            server content
            """
        And a conflict file for 'lorem.txt' should exist on the file system with following content
            """
            client content
            """