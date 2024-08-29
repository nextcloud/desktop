Feature: remove account connection
  As a user
  I want to remove my account
  So that I won't be using any client-UI services


    Scenario: remove an account connection
        Given user "Alice" has been created in the server with default attributes
        And user "Brian" has been created in the server with default attributes
        And the user has set up the following accounts with default settings:
            | Alice |
            | Brian |
        When the user removes the connection for user "Brian" and host %local_server_hostname%
        Then the account with displayname "Brian Murphy" and host "%local_server_hostname%" should not be displayed
        But the account with displayname "Alice Hansen" and host "%local_server_hostname%" should be displayed


    Scenario: remove the only account connection
        Given user "Alice" has been created in the server with default attributes
        And user "Alice" has set up a client with default settings
        When the user removes the connection for user "Alice" and host %local_server_hostname%
        Then connection wizard should be visible
