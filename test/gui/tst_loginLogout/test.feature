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
