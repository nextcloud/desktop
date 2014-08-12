tell application "Finder"
    try
        «event NVTYlded»
        set the result to 0
    on error msg number code
        set the result to code
    end try
end tell

