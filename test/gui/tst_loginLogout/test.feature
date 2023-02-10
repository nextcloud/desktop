Feature:  Logout users
  As a user
  I want to be able to login and logout of my account
  So that I can protect my work and identity and be assured of privacy

  Background:
    Given user "Alice" has been created on the server with default attributes and without skeleton files

  Scenario: logging out
    Given user "Alice" has set up a client with default settings
    When the user "Alice" logs out of the client-UI
    Then user "Alice" should be signed out

  Scenario: login after loggin out
    Given user "Alice" has set up a client with default settings
    And user "Alice" has logged out of the client-UI
    When user "Alice" logs in to the client-UI
    Then user "Alice" should be connect to the client-UI

  Scenario: login with oauth2 enabled
      Given app "oauth2" has been "enabled" on the server
      And the user has started the client
      When the user adds the following account with oauth2 enabled:
          | server   | %local_server% |
          | user     | Alice          |
          | password | 12345          |
      Then the account with displayname "Alice Hansen" and host "%local_server_hostname%" should be displayed
