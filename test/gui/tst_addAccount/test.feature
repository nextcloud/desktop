Feature: adding accounts

    As a user
    I want to be able join multiple owncloud servers to the client
    So that I can sync data with various organisations


    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files


    Scenario: Check default options in advanced configuration
        Given the user has started the client
        When the user adds the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        And the user opens the advanced configuration
        Then the download everything option should be selected by default
        And the user should be able to choose the local download directory


    Scenario: Adding normal Account
        Given the user has started the client
        When the user adds the first account with
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        Then the account with displayname "Alice Hansen" and host "%local_server_hostname%" should be displayed


    Scenario: Adding multiple accounts
        Given user "Brian" has been created on the server with default attributes and without skeleton files
        And user "Alice" has set up a client with default settings
        When the user adds another account with
            | server   | %local_server% |
            | user     | Brian          |
            | password | AaBb2Cc3Dd4    |
        Then the account with displayname "Alice Hansen" and host "%local_server_hostname%" should be displayed
        And the account with displayname "Brian Murphy" and host "%local_server_hostname%" should be displayed


    @skipOnOCIS
    Scenario: Adding account with wrong credentials
        Given the user has started the client
        And the user has added the server "%local_server%"
        When the user adds the following wrong user credentials:
            | user     | Alice |
            | password | 12345 |
        Then error "Login failed: username and/or password incorrect" should be displayed


    Scenario: Adding account with self signed certificate for the first time
        Given the user has started the client
        When the user adds the server "%secure_local_server%"
        And the user accepts the certificate
        Then credentials wizard should be visible


    Scenario: Adding account with vfs enabled
        Given the user has started the client
        And the user has added the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        When the user selects vfs option in advanced section
        And the user confirms the enable experimental vfs option
        Then VFS enabled baseline image should match the default screenshot


    Scenario: Try to enable experimental vfs option and cancel it
        Given the user has started the client
        And the user has added the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        When the user selects vfs option in advanced section
        And the user cancels the enable experimental vfs option
        Then the account with displayname "Alice Hansen" and host "%local_server_hostname%" should be displayed
        And VFS enabled baseline image should not match the default screenshot
