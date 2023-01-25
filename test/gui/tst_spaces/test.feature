@skipOnOC10
Feature: Project spaces
    As a user
    I want to sync project space
    So that I can do view and manage the space

    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files
        And the administrator has created a space "Project 101"


    Scenario: User with viewer role can open the file
        Given the administrator has created a folder "planning" in space "Project 101"
        And the administrator has uploaded a file "testfile.txt" with content "some content" inside space "Project 101"
        And the administrator has added user "Alice" to space "Project 101" with role "viewer"
        And user "Alice" has set up a client with space "Project 101"
        When user "Alice" opens the file "testfile.txt" in space "Project 101"
        Then the file "testfile.txt" should exist in space "Project 101"