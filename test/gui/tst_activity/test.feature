Feature: filter activity for user
    As a user
    I want to filter activity
    So that I can view activity of specific user


    Scenario: filter synced activities
        Given user "Alice" has been created on the server with default attributes and without skeleton files
        And user "Brian" has been created on the server with default attributes and without skeleton files
        And user "Alice" has created folder "simple-folder" on the server
        And the user has set up the following accounts with default settings:
            | Alice |
            | Brian |
        When the user clicks on the activity tab
        And the user selects "Local Activity" tab in the activity
        And the user checks the activities of account "Alice Hansen@%local_server_hostname%"
        Then the following activities should be displayed in synced table
            | resource      | action     | account                              |
            | simple-folder | Downloaded | Alice Hansen@%local_server_hostname% |


    Scenario: filter not synced activities
        Given user "Alice" has been created on the server with default attributes and without skeleton files
        And user "Alice" has created a folder "Folder1" inside the sync folder
        And user "Alice" has set up a client with default settings
        When user "Alice" creates the following files inside the sync folder:
            | files             |
            | /.htaccess        |
            | /Folder1/a\\a.txt |
        And the user clicks on the activity tab
        And the user selects "Not Synced" tab in the activity
        Then the file "Folder1/a\\a.txt" should be blacklisted
        And the file ".htaccess" should be excluded
        When the user unchecks the "Excluded" filter
        Then the following activities should be displayed in not synced table
            | resource         | status      | account                              |
            | Folder1/a\\a.txt | Blacklisted | Alice Hansen@%local_server_hostname% |
