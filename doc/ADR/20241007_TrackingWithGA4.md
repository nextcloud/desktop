# Use Tracking with Google Analytics 4 

## Status

proposed

## Context

The web front end already tracks user interactions with GA4. To track interactions in the client software as well, an implementation with GA should also be done here. 

## Decision

Since the use of GA4 in desktop software and the use of the pure API are not natively supported, a reverse-engineered solution had to be used (see https://ga4mp.dev/#/), similar to the use of the GA4 Measurement Protocol, which, however, does not provide a complete replacement.

This approach implements GA4 tracking based on HTTP POST requests.

## Implementation Details

The implementation consists of three classes:

* GAnalyticsWorker
* GAnalytics
* DataCollectionWrapper

The DataCollectionWrapper is the outermost layer and is used by the application to track actions and events. The DataCollectionWrapper provides enums for various tracking pages (areas of the application, such as GeneralSettings or UserSettings) and tracking elements (specific buttons, checkboxes, or similar items).
GAnalytics acts as an intermediary between the outer layer (DataCollectionWrapper) and the communication logic. Various variables are also set here. When a tracking call is made, it is forwarded to the GAnalyticsWorker, where it is queued.

The GAnalyticsWorker contains a queue, a message loop, and a QNetworkAccessManager. At fixed intervals, the queue is checked for tracking calls, which are then sent to the GA4 interface. If multiple calls are present, the connection is kept alive until all tracking calls have been sent.

## Consequences
This approach allows client-side tracking using GA4, bridging the gap between web and desktop tracking. The modular design simplifies maintenance by separating tracking logic from communication handling. Usage is straightforward; the application simply calls the DataCollectionWrapper at points where tracking should occur, ensuring that actions and events are recorded seamlessly. However, relying on a reverse-engineered solution introduces risks of future incompatibility with GA4 updates. Managing a queue and network connection adds complexity, and there might be latency when batching tracking calls. Overall, the solution balances functionality with maintainability but carries some technical risks.
