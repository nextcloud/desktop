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
