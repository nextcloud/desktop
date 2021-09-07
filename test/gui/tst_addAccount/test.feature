Feature: adding accounts

    As a user
    I want to be able join multiple owncloud servers to the client
    So that I can sync data with various organisations


	Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files

    Scenario: Adding normal Account
        Given the user has started the client
        When the user adds the first account with
          | server      | %local_server%           |
          | user        | Alice                    |
          | password    | 1234                     |
          | localfolder | %client_sync_path_user1% |
        Then an account should be displayed with the displayname Alice Hansen and host %local_server_hostname%


     Scenario: Adding multiple accounts
        Given user "Brian" has been created on the server with default attributes and without skeleton files
        And user "Alice" has set up a client with default settings
        When the user adds an account with
          | server      | %local_server%           |
          | user        | Brian                    |
          | password    | AaBb2Cc3Dd4              |
          | localfolder | %client_sync_path_user2% |
        Then an account should be displayed with the displayname Alice Hansen and host %local_server_hostname%
        And an account should be displayed with the displayname Brian Murphy and host %local_server_hostname%
