Feature: remove account connection

  As a user
  I want to remove my account
  So that I won't be using any client-UI services


  Scenario: remove an account connection
    Given user "Alice" has been created on the server with default attributes and without skeleton files
    And user "Brian" has been created on the server with default attributes and without skeleton files
    And user "Alice" has set up a client with default settings
    And the user has added another account with
      | server   | %local_server% |
      | user     | Brian          |
      | password | AaBb2Cc3Dd4    |
    When the user removes the connection for user "Brian" and host %local_server_hostname%
    Then the account with displayname "Brian Murphy" and host "%local_server_hostname%" should not be displayed
    But the account with displayname "Alice Hansen" and host "%local_server_hostname%" should be displayed


  Scenario: remove the only account connection
    Given user "Alice" has been created on the server with default attributes and without skeleton files
    And user "Alice" has set up a client with default settings
    When the user removes the connection for user "Alice" and host %local_server_hostname%
    Then connection wizard should be visible
