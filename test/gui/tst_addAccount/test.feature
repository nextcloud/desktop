Feature: adding accounts

    As a user
    I want to be able join multiple owncloud servers to the client
    So that I can sync data with various organisations


    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files


    Scenario: Check default options in advanced configuration
        Given the user has started the client
        And the user has entered the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        When the user opens the advanced configuration
        Then the download everything option should be selected by default
        And the user should be able to choose the local download directory


    Scenario: Adding normal Account
        Given the user has started the client
        When the user adds the following account:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        Then the account with displayname "Alice Hansen" and host "%local_server_hostname%" should be displayed


    Scenario: Adding multiple accounts
        Given user "Brian" has been created on the server with default attributes and without skeleton files
        And user "Alice" has set up a client with default settings
        When the user opens the add-account dialog
        And the user adds the following account:
            | server   | %local_server% |
            | user     | Brian          |
            | password | AaBb2Cc3Dd4    |
        Then the account with displayname "Alice Hansen" and host "%local_server_hostname%" should be displayed
        And the account with displayname "Brian Murphy" and host "%local_server_hostname%" should be displayed


    @skipOnOCIS
    Scenario: Adding account with wrong credentials
        Given the user has started the client
        And the user has entered the following account information:
            | server   | %local_server% |
        When the user adds the following wrong user credentials:
            | user     | Alice |
            | password | 12345 |
        Then error "Invalid credentials" should be displayed


    Scenario: Adding account with self signed certificate for the first time
        Given the user has started the client
        When the user adds the server "%secure_local_server%"
        And the user accepts the certificate
        Then credentials wizard should be visible


    @skip @issue-11393
    Scenario: Adding account with vfs enabled
        Given the user has started the client
        And the user has entered the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        When the user selects vfs option in advanced section
        And the user confirms the enable experimental vfs option
        Then VFS enabled baseline image should match the default screenshot


    @skip @issue-11393
    Scenario: Try to enable experimental vfs option and cancel it
        Given the user has started the client
        And the user has entered the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        When the user selects vfs option in advanced section
        And the user cancels the enable experimental vfs option
        Then the account with displayname "Alice Hansen" and host "%local_server_hostname%" should be displayed
        And VFS enabled baseline image should not match the default screenshot

    @skipOnOC10
    Scenario: Add space manually from sync connection window
        Given user "Alice" has created folder "simple-folder" in the server
        And the user has started the client
        And the user has entered the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        When the user selects manual sync folder option in advanced section
        And the user syncs the "Personal" space
        Then the folder "simple-folder" should exist on the file system

