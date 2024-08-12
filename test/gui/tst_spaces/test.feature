@skipOnOC10
Feature: Project spaces
    As a user
    I want to sync project space
    So that I can do view and manage the space

    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files
        And the administrator has created a space "Project101"


    Scenario: User with Viewer role can open the file
        Given the administrator has created a folder "planning" in space "Project101"
        And the administrator has uploaded a file "testfile.txt" with content "some content" inside space "Project101"
        And the administrator has added user "Alice" to space "Project101" with role "viewer"
        And user "Alice" has set up a client with space "Project101"
        Then user "Alice" should be able to open the file "testfile.txt" on the file system
        And as "Alice" the file "testfile.txt" should have content "some content" on the file system

    @skipOnWindows
    Scenario: User with Viewer role cannot edit the file (Linux only)
        Given the administrator has created a folder "planning" in space "Project101"
        And the administrator has uploaded a file "testfile.txt" with content "some content" inside space "Project101"
        And the administrator has added user "Alice" to space "Project101" with role "viewer"
        And user "Alice" has set up a client with space "Project101"
        Then user "Alice" should not be able to edit the file "testfile.txt" on the file system
        And as "Alice" the file "testfile.txt" in the space "Project101" should have content "some content" in the server

    @skipOnLinux
    Scenario: User with Viewer role cannot edit the file (Windows only)
        Given the administrator has created a folder "planning" in space "Project101"
        And the administrator has uploaded a file "testfile.txt" with content "some content" inside space "Project101"
        And the administrator has added user "Alice" to space "Project101" with role "viewer"
        And user "Alice" has set up a client with space "Project101"
        When the user overwrites the file "testfile.txt" with content "overwrite some content"
        And the user clicks on the activity tab
        And the user selects "Not Synced" tab in the activity
        Then the file "testfile.txt" should be blacklisted
        And as "Alice" the file "testfile.txt" in the space "Project101" should have content "some content" in the server


    Scenario: User with Editor role can edit the file
        Given the administrator has created a folder "planning" in space "Project101"
        And the administrator has uploaded a file "testfile.txt" with content "some content" inside space "Project101"
        And the administrator has added user "Alice" to space "Project101" with role "editor"
        And user "Alice" has set up a client with space "Project101"
        When the user overwrites the file "testfile.txt" with content "some content edited"
        And the user waits for file "testfile.txt" to be synced
        Then as "Alice" the file "testfile.txt" in the space "Project101" should have content "some content edited" in the server


    Scenario: User with Manager role can add files and folders
        Given the administrator has added user "Alice" to space "Project101" with role "manager"
        And user "Alice" has set up a client with space "Project101"
        When user "Alice" creates a file "localFile.txt" with the following content inside the sync folder
            """
            test content
            """
        And user "Alice" creates a folder "localFolder" inside the sync folder
        And the user waits for the files to sync
        Then as "Alice" the file "localFile.txt" in the space "Project101" should have content "test content" in the server
        And as "Alice" the space "Project101" should have folder "localFolder" in the server
