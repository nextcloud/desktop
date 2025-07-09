# NextcloudFileProviderKit

NextcloudFileProviderKit is a Swift package designed to simplify the development of Nextcloud synchronization applications on Apple devices using the [File Provider Framework](https://developer.apple.com/documentation/FileProvider). This package provides the core functionality for virtual files in the macOS Nextcloud client, making it easier for developers to integrate Nextcloud syncing capabilities into their applications.

NextcloudFileProviderKit depends on NextcloudKit to communicate with the server.

## Features

- **Easy Integration**: Seamlessly integrate Nextcloud syncing into your Apple applications using the FileProvider API.
- **Core Functionality**: Provides the essential features needed for handling virtual files, including fetching contents, creating, modifying, and deleting items.
- **macOS Support**: Used as the core functionality package for virtual files in the macOS Nextcloud client.

## Installation

To install NextcloudFileProviderKit, add the following to your `Package.swift`:

```swift
dependencies: [
    .package(url: "https://github.com/nextcloud/NextcloudFileProviderKit.git", from: "1.0.0")
]
```

## Usage

To use NextcloudFileProviderKit in your application, you can refer to the following example code that demonstrates how to implement various functionalities of the `FileProviderExtension` class.

### Initialization

```swift
import NextcloudKit
import NextcloudFileProviderKit

let ncKit = NextcloudKit()
ncKit.setup(
    account: "username https://cloud.mycloud.com",
    user: "username",
    userId: "username",
    password: "password",
    urlBase: "https://cloud.mycloud.com"
)

```

### Fetching Item

```swift
func item(
    for identifier: NSFileProviderItemIdentifier,
    request _: NSFileProviderRequest,
    completionHandler: @escaping (NSFileProviderItem?, Error?) -> Void
) -> Progress {
    if let item = Item.storedItem(identifier: identifier, remoteInterface: ncKit) {
        completionHandler(item, nil)
    } else {
        completionHandler(
            nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: identifier)
        )
    }
    return Progress()
}
```

### Fetching Contents

```swift
func fetchContents(
    for itemIdentifier: NSFileProviderItemIdentifier,
    version requestedVersion: NSFileProviderItemVersion?,
    request: NSFileProviderRequest,
    completionHandler: @escaping (URL?, NSFileProviderItem?, Error?) -> Void
) -> Progress {
    guard requestedVersion == nil else {
        completionHandler(
            nil,
            nil,
            NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError)
        )
        return Progress()
    }

    guard let item = Item.storedItem(identifier: itemIdentifier, remoteInterface: ncKit) else {
        completionHandler(
            nil, nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier)
        )
        return Progress()
    }

    let progress = Progress()
    Task {
        let (localUrl, updatedItem, error) = await item.fetchContents(domain: self.domain, progress: progress)
        completionHandler(localUrl, updatedItem, error)
    }
    return progress
}
```

### Creating Item

```swift
func createItem(
    basedOn itemTemplate: NSFileProviderItem,
    fields: NSFileProviderItemFields,
    contents url: URL?,
    options: NSFileProviderCreateItemOptions = [],
    request: NSFileProviderRequest,
    completionHandler: @escaping (NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?) -> Void
) -> Progress {
    let progress = Progress()
    Task {
        let (item, error) = await Item.create(
            basedOn: itemTemplate,
            fields: fields,
            contents: url,
            request: request,
            domain: self.domain,
            remoteInterface: ncKit,
            progress: progress
        )
        completionHandler(item ?? itemTemplate, NSFileProviderItemFields(), false, error)
    }
    return progress
}
```

### Modifying Item

```swift
func modifyItem(
    _ item: NSFileProviderItem,
    baseVersion: NSFileProviderItemVersion,
    changedFields: NSFileProviderItemFields,
    contents newContents: URL?,
    options: NSFileProviderModifyItemOptions = [],
    request: NSFileProviderRequest,
    completionHandler: @escaping (NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?) -> Void
) -> Progress {
    guard let existingItem = Item.storedItem(identifier: item.itemIdentifier, remoteInterface: ncKit) else {
        completionHandler(
            item,
            [],
            false,
            NSError.fileProviderErrorForNonExistentItem(withIdentifier: item.itemIdentifier)
        )
        return Progress()
    }

    let progress = Progress()
    Task {
        let (modifiedItem, error) = await existingItem.modify(
            itemTarget: item,
            baseVersion: baseVersion,
            changedFields: changedFields,
            contents: newContents,
            options: options,
            request: request,
            domain: domain,
            progress: progress
        )
        completionHandler(modifiedItem ?? item, [], false, error)
    }
    return progress
}
```

### Deleting Item

```swift
func deleteItem(
    identifier: NSFileProviderItemIdentifier,
    baseVersion _: NSFileProviderItemVersion,
    options _: NSFileProviderDeleteItemOptions = [],
    request _: NSFileProviderRequest,
    completionHandler: @escaping (Error?) -> Void
) -> Progress {
    guard let item = Item.storedItem(identifier: identifier, remoteInterface: ncKit) else {
        completionHandler(NSError.fileProviderErrorForNonExistentItem(withIdentifier: identifier))
        return Progress()
    }

    let progress = Progress(totalUnitCount: 1)
    Task {
        let error = await item.delete()
        progress.completedUnitCount = 1
        completionHandler(await item.delete())
    }
    return progress
}
```

### Enumerator

```swift
func enumerator(
    for containerItemIdentifier: NSFileProviderItemIdentifier,
    request _: NSFileProviderRequest
) throws -> NSFileProviderEnumerator {
    return Enumerator(
        enumeratedItemIdentifier: containerItemIdentifier,
        remoteInterface: ncKit,
        domain: domain
    )
}
```

## Contributing

Contributions are welcome! Please feel free to submit a pull request or open an issue if you encounter any problems or have suggestions for improvements.

## License

This project is licensed under the LGPLv3 License. See the [LICENSE](LICENSE) file for more details.

---
