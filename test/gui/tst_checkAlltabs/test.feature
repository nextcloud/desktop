Feature: Visually check all tabs

    As a user
    I want to visually check all tabs in client
    So that I can performe all the actions related to client


    Scenario: Tabs in toolbar looks correct
        Given user "Alice" has been created on the server with default attributes and without skeleton files
        And user "Alice" has set up a client with default settings
        Then the following tabs in the toolbar should match the default baseline
            | AddAccount   |
            | Activity     |
            | Settings     |
            | QuitOwncloud |

    @skip
    Scenario: Open log dialog with Ctrl+l keys combination
        Given user "Alice" has been created on the server with default attributes and without skeleton files
        And user "Alice" has set up a client with default settings
        When the user presses the "Ctrl+l" keys
        Then the log dialog should be opened