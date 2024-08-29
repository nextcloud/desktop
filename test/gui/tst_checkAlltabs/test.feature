Feature: Visually check all tabs
    As a user
    I want to visually check all tabs in client
    So that I can performe all the actions related to client


    Scenario: Tabs in toolbar looks correct
        Given user "Alice" has been created in the server with default attributes
        And user "Alice" has set up a client with default settings
        Then the toolbar should have the following tabs:
            | Add Account |
            | Activity    |
            | Settings    |
            | Quit        |


    Scenario: Verify various setting options in Settings tab
        Given user "Alice" has been created in the server with default attributes
        And user "Alice" has set up a client with default settings
        When the user clicks on the settings tab
        Then the settings tab should have the following options in the general section:
            | Start on Login                           |
            | Use Monochrome Icons in the system tray  |
            | Show desktop Notifications               |
            | Language                                 |
        And the settings tab should have the following options in the advanced section:
            | Sync hidden files                                                    |
            | Edit ignored files                                                   |
            | Log settings                                                         |
        And the settings tab should have the following options in the network section:
            | Proxy Settings     |
            | Download Bandwidth |
            | Upload Bandwidth   |
        When the user opens the about dialog
        Then the about dialog should be opened
