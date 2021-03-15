Feature: adding accounts

    As a user
    I want to be able join multiple owncloud servers to the client
    So that I can sync data with various organisations

    Scenario: Adding normal Account
        Given user "Alice" has been created on the server with default attributes
        And the user has started the client
        When the user adds the first account with
            | server      | %local_server%     |
            | user        | Alice              |
            | password    | 1234               |
            | localfolder | %client_sync_path% |
        Then an account should be displayed with the displayname Alice Hansen and host %local_server_hostname%

     Scenario: Adding multiple account
        Given user "Brian" has been created on the server with default attributes
        And user "Alice" has been created on the server with default attributes
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
            0\display-name=Alice Hansen
            0\http_oauth=false
            0\http_user=alice
            0\url=%local_server%
            0\user=Alice
            0\version=1
            version=2
            """
        When the user adds an account with
            | server      | %local_server%     |
            | user        | Brian              |
            | password    | AaBb2Cc3Dd4        |
            | localfolder | %client_sync_path% |
        Then an account should be displayed with the displayname Alice Hansen and host %local_server_hostname%
        And an account should be displayed with the displayname Brian Murphy and host %local_server_hostname%
