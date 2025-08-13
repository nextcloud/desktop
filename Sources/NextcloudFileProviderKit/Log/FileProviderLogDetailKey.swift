///
/// A predefined set of detail keys to avoid having multiple keys for the same type of information accidentally while still leaving the possibility to define arbitrary keys.
///
public enum FileProviderLogDetailKey: String {
    ///
    /// The identifier for an account.
    ///
    case account

    ///
    /// The raw value of an `NSFileProviderDomainIdentifier`.
    ///
    case domain

    ///
    /// The original and underlying error.
    ///
    /// Use this for any `Error` or `NSError`, the logging system will extract relevant values in a central place automatically.
    ///
    case error

    ///
    /// HTTP entity tag.
    ///
    /// See [Wikipedia](https://en.wikipedia.org/wiki/HTTP_ETag) for further information.
    ///
    case eTag

    ///
    /// The raw value of an `NSFileProviderItemIdentifier`.
    ///
    case item

    ///
    /// A ``SendableItemMetadata`` object.
    ///
    /// This will automatically encode all important properties as a dictionary in the log.
    /// Always prefer this over individual log detail arguments to keep the call points concise.
    ///
    case metadata

    ///
    /// The name of a file or directory in the file system.
    ///
    case name

    ///
    /// The server-side item identifier.
    ///
    case ocId

    ///
    /// The last time item metadata was synchronized with the server.
    ///
    case syncTime

    ///
    /// Any relevant URL, in example in context of a network request.
    ///
    case url
}
