import Foundation

@discardableResult
func run(
    _ launchPath: String, 
    _ args: [String], 
    env: [String: String]? = nil, 
    quiet: Bool = false
) -> Int32 {
    let task = Process()
    task.launchPath = launchPath
    task.arguments = args
    
    if let env, 
       let combinedEnv = task.environment?.merging(env, uniquingKeysWith: { (_, new) in new })
    {
        task.environment = combinedEnv
    }
    
    if quiet {
        task.standardOutput = nil
        task.standardError = nil
    }

    task.launch()
    task.waitUntilExit()
    return task.terminationStatus
}

func run(
    _ launchPath: String, 
    _ args: String..., 
    env: [String: String]? = nil, 
    quiet: Bool = false
) -> Int32 {
    return run(launchPath, args, env: env, quiet: quiet)
}

@discardableResult
func shell(_ commands: String..., env: [String: String]? = nil, quiet: Bool = false) -> Int32 {
    return run("/bin/zsh", ["-c"] + commands, env: env, quiet: quiet)
}

func commandExists(_ command: String) -> Bool {
    return run("/usr/bin/type", command, quiet: true) == 0
}

print("Configuring build tooling.")

if commandExists("git") {
    print("Git is installed.")
} else {
    print("Git is missing. Installing xcode command line tools.")
    guard shell("xcode-select --install") == 0 else {
        print("Failed to install xcode command line tools.")
        exit(1)
    }
}

if commandExists("brew") {
    print("Brew is installed.")
} else {
    print("Installing Homebrew...")
    guard shell(
        "curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh | /bin/bash", 
        env: ["NONINTERACTIVE": "1"]
    ) == 0 else {
        print("Failed to install Homebrew.")
        exit(1)
    }
    print("Brew installed.")
}

if commandExists("inkscape") {
    print("Inkscape is installed.")
} else {
    guard shell("brew install inkscape") == 0 else {
        print("Failed to install Inkscape.")
        exit(1)
    }
    print("Inkscape installed.")
}

if commandExists("python3") {
    print("Python 3 is installed.")
} else {
    if commandExists("pyenv") {
        print("Pyenv is installed.")
    } else {
        guard shell("brew install pyenv") == 0 else {
            print("Failed to install Pyenv.")
            exit(1)
        }
    }

    guard shell ("pyenv install 3.12.4") == 0 else {
        print("Failed to install Python 3.")
        exit(1)
    }
}
