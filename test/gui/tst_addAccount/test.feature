Feature: adding accounts

    As a user
    I want to be able join multiple owncloud servers to the client
    So that I can sync data with various organisations


    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files

    Scenario: Adding normal Account
        Given the user has started the client
        When the user adds the first account with
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        Then an account should be displayed with the displayname Alice Hansen and host %local_server_hostname%


    Scenario: Adding multiple accounts
        Given user "Brian" has been created on the server with default attributes and without skeleton files
        And user "Alice" has set up a client with default settings
        When the user adds another account with
            | server   | %local_server% |
            | user     | Brian          |
            | password | AaBb2Cc3Dd4    |
        Then an account should be displayed with the displayname Alice Hansen and host %local_server_hostname%
        And an account should be displayed with the displayname Brian Murphy and host %local_server_hostname%

    @skip
    Scenario: Adding account with wrong credentials
        Given the user has started the client
        And the user has added the following server address:
            | server | %local_server% |
        When the user adds the following wrong user credentials:
            | user     | Alice |
            | password | 12345 |
        Then error "Login failed: username and/or password incorrect" should be displayed

    @skip
    Scenario: Adding account with self signed certificate for the first time
        Given the user has started the client
        When the user adds the following server address:
            | server | %secure_local_server% |
        And the user accepts the certificate
        Then credentials wizard should be visible


    Scenario: Adding account with virtual files enabled through advance configuration
        Given the user has started the client
        When the user adds the first account with advanced configuration
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        And the user selects virtual_files option in advanced section
        And the user selects enable_experimental_placeholder_mode option in enable experimental feature dialogue box
        Then VFS enabled baseline image should match the default screenshot


    Scenario: Adding account with stay safe advance configuration
        Given the user has started the client
        When the user adds the first account with advanced configuration
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        And the user selects virtual_files option in advanced section
        And the user selects stay_safe option in enable experimental feature dialogue box
        Then an account should be displayed with the displayname Alice Hansen and host %local_server_hostname%
        And VFS enabled baseline image should not match the default screenshot
