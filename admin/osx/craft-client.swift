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

func installIfMissing(
    _ command: String, 
    _ installCommand: String, 
    installCommandEnv: [String: String]? = nil
) {
    if commandExists(command) {
        print("\(command) is installed.")
    } else {
        print("\(command) is missing. Installing...")
        guard shell(installCommand, env: installCommandEnv) == 0 else {
            print("Failed to install \(command).")
            exit(1)
        }
        print("\(command) installed.")
    } 
}

print("Configuring build tooling.")

installIfMissing("git", "xcode-select --install")
installIfMissing(
    "brew", 
    "curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh | /bin/bash", 
    installCommandEnv: ["NONINTERACTIVE": "1"]
)
installIfMissing("inkscape", "brew install inkscape")
installIfMissing("python3", "brew install pyenv && pyenv install 3.12.4")

print("Build tooling configured.")
print("Configuring KDE Craft.")

let fm = FileManager.default
let currentDir = fm.currentDirectoryPath
let craftDir = "\(currentDir)/craftmaster"

if fm.fileExists(atPath: craftDir) {
    print("KDE Craft is already cloned.")
} else {
    print("Cloning KDE Craft...")
    shell("git clone -q --depth=1 https://invent.kde.org/packaging/craftmaster.git \(craftDir)")
}
