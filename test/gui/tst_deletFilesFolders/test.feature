Feature: deleting files and folders
  	As a user
  	I want to delete files and folders
  	So that I can keep my filing system clean and tidy

    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files

    @issue-9439
    Scenario Outline: Delete a file
        Given user "Alice" has uploaded file with content "ownCloud test text file 0" to "<fileName>" in the server
        And user "Alice" has set up a client with default settings
        When the user deletes the file "<fileName>"
        And the user waits for the files to sync
        Then as "Alice" file "<fileName>" should not exist in the server
        Examples:
            | fileName                                    |
            | textfile0.txt                               |
            | textfile0-with-name-more-than-20-characters |
            |  ~`!@#$^&()-_=+{[}];',textfile.txt	  |

    @issue-9439
    Scenario Outline: Delete a folder
        Given user "Alice" has created folder "<folderName>" in the server
        And user "Alice" has set up a client with default settings
        When the user deletes the folder "<folderName>"
        And the user waits for the files to sync
        Then as "Alice" file "<folderName>" should not exist in the server
        Examples:
            | folderName                                      |
            | simple-empty-folder                             |
            | simple-folder-with-name-more-than-20-characters |


    Scenario: Delete a file and a folder
        Given user "Alice" has uploaded file with content "test file 1" to "textfile1.txt" in the server
        And user "Alice" has uploaded file with content "test file 2" to "textfile2.txt" in the server
        And user "Alice" has created folder "test-folder1" in the server
        And user "Alice" has created folder "test-folder2" in the server
        And user "Alice" has set up a client with default settings
        When the user deletes the file "textfile1.txt"
        And the user deletes the folder "test-folder1"
        And the user waits for the files to sync
        Then as "Alice" file "textfile1.txt" should not exist in the server
        And as "Alice" folder "test-folder1" should not exist in the server
        And as "Alice" file "textfile2.txt" should exist in the server
        And as "Alice" folder "test-folder2" should exist in the server
