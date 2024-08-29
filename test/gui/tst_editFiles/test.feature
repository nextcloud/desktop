Feature: edit files
    As a user
    I want to be able to edit the file content
    So that I can modify and change file data

    Background:
        Given user "Alice" has been created in the server with default attributes


    Scenario: Modify orignal content of a file with special character
        Given user "Alice" has uploaded file with content "ownCloud test text file 0" to "S@mpleFile!With,$pecial&Characters.txt" in the server
        And user "Alice" has set up a client with default settings
        When the user overwrites the file "S@mpleFile!With,$pecial&Characters.txt" with content "overwrite ownCloud test text file"
        And the user waits for file "S@mpleFile!With,$pecial&Characters.txt" to be synced
        Then as "Alice" the file "S@mpleFile!With,$pecial&Characters.txt" should have the content "overwrite ownCloud test text file" in the server
