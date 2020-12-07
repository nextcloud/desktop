Feature: Sharing

    Scenario: simple sharing
        Given user 'Alice' has been created with default attributes
        And user 'Brian' has been created with default attributes
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
            """
        When the user adds "Brian" as collaborator of resource "%client_sync_path%/textfile0.txt" with permissions "edit,share" using the client-UI
        Then user "Brian" should be listed in the collaborators list for file "%client_sync_path%/textfile0.txt" with permissions "edit,share" on the client-UI
