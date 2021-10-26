Feature: Syncing files

    As a user
    I want to be able to sync my local folders to to my owncloud server
    so that I dont have to upload and download files manually

    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files

    @smokeTest
    Scenario: Syncing a file to the server
        Given user "Alice" has set up a client with default settings
        When the user creates a file "lorem-for-upload.txt" with the following content on the file system
            """
            test content
            """
        And the user waits for file "lorem-for-upload.txt" to be synced
        Then as "Alice" the file "lorem-for-upload.txt" on the server should have the content "test content"


    Scenario: Syncing all files and folders from the server
        Given user "Alice" has created folder "simple-folder" on the server
        And user "Alice" has created folder "large-folder" on the server
        And user "Alice" has uploaded file on the server with content "test content" to "uploaded-lorem.txt"
        And user "Alice" has set up a client with default settings
        When the user waits for the files to sync
        Then the file "uploaded-lorem.txt" should exist on the file system
        And the file "uploaded-lorem.txt" should exist on the file system with the following content
            """
            test content
            """
        And the folder "simple-folder" should exist on the file system
        And the folder "large-folder" should exist on the file system


    Scenario: Syncing a file from the server and creating a conflict
        Given user "Alice" has uploaded file on the server with content "server content" to "/conflict.txt"
        And user "Alice" has set up a client with default settings
        And the user has waited for file "conflict.txt" to be synced
        And the user has paused the file sync
        And the user has changed the content of local file "conflict.txt" to:
            """
            client content
            """
        And user "Alice" has uploaded file on the server with content "changed server content" to "/conflict.txt"
        When the user resumes the file sync on the client
        And the user clicks on the activity tab
        And the user selects "Not Synced" tab in the activity
        # Then a conflict warning should be shown for 1 files
        Then the table of conflict warnings should include file "conflict.txt"
        And the file "conflict.txt" should exist on the file system with the following content
            """
            changed server content
            """
        And a conflict file for "conflict.txt" should exist on the file system with the following content
            """
            client content
            """


    Scenario: Sync all is selected by default
        Given user "Alice" has created folder "simple-folder" on the server
        And user "Alice" has created folder "large-folder" on the server
        And the user has started the client
        And the user has added the following account information:
            | server      | %local_server%           |
            | user        | Alice                    |
            | password    | 1234                     |
            | localfolder | %client_sync_path_user1% |
        When the user opens chose_what_to_sync dialog
        Then the dialog chose_what_to_sync should be visible
        And the sync all checkbox should be checked


    Scenario: Sync only one folder from the server
        Given user "Alice" has created folder "simple-folder" on the server
        And user "Alice" has created folder "large-folder" on the server
        And the user has started the client
        And the user has added the following account information:
            | server      | %local_server%           |
            | user        | Alice                    |
            | password    | 1234                     |
            | localfolder | %client_sync_path_user1% |
        When the user selects the following folders to sync:
            | folder        |
            | simple-folder |
        And the user connects the account
        And the user waits for the files to sync
        Then the folder "simple-folder" should exist on the file system
        But the folder "large-folder" should not exist on the file system


    Scenario: Connect account with manual sync folder option
        Given user "Alice" has created folder "simple-folder" on the server
        And user "Alice" has created folder "large-folder" on the server
        And user "Alice" has uploaded file on the server with content "test content" to "lorem.txt"
        And the user has started the client
        And the user has added the following account information:
            | server      | %local_server%           |
            | user        | Alice                    |
            | password    | 1234                     |
            | localfolder | %client_sync_path_user1% |
        When the user selects manual sync folder option
        And the user connects the account
        Then the folder "simple-folder" should not exist on the file system
        But the folder "large-folder" should not exist on the file system
        And the file "lorem.txt" should not exist on the file system


    Scenario: sort folders list by name and size
        Given user "Alice" has created folder "123Folder" on the server
        And user "Alice" has uploaded file on the server with content "small" to "123Folder/lorem.txt"
        And user "Alice" has created folder "aFolder" on the server
        And user "Alice" has uploaded file on the server with content "more contents" to "aFolder/lorem.txt"
        And user "Alice" has created folder "bFolder" on the server
        And the user has started the client
        And the user has added the following account information:
            | server      | %local_server%           |
            | user        | Alice                    |
            | password    | 1234                     |
            | localfolder | %client_sync_path_user1% |
        When the user opens chose_what_to_sync dialog
        # folders are sorted by name in ascending order by default
        Then the folders should be in the following order:
            | folder    |
            | 123Folder |
            | aFolder   |
            | bFolder   |
        # sort folder by name in descending order
        When the user sorts the folder list by "Name"
        Then the folders should be in the following order:
            | folder    |
            | bFolder   |
            | aFolder   |
            | 123Folder |
        # sort folder by size in ascending order
        When the user sorts the folder list by "Size"
        Then the folders should be in the following order:
            | folder    |
            | bFolder   |
            | 123Folder |
            | aFolder   |
        # sort folder by size in descending order
        When the user sorts the folder list by "Size"
        Then the folders should be in the following order:
            | folder    |
            | aFolder   |
            | 123Folder |
            | bFolder   |