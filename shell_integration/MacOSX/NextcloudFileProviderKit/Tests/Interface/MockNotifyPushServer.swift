//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import NIOCore
import NIOHTTP1
import NIOPosix
import NIOWebSocket

@available(macOS 14, iOS 17, tvOS 17, watchOS 10, *)
public class MockNotifyPushServer: @unchecked Sendable {
    /// The server's host.
    private let host: String
    /// The server's port.
    private let port: Int
    /// The server's event loop group.
    private let eventLoopGroup: MultiThreadedEventLoopGroup

    private let username: String
    private let password: String
    private var usernameReceived = false
    private var passwordReceived = false
    private var connectedClients: [NIOAsyncChannel<WebSocketFrame, WebSocketFrame>] = []
    public var delay: Int?
    public var refuse = false
    public var pingHandler: (() -> Void)?

    enum UpgradeResult {
        case websocket(NIOAsyncChannel<WebSocketFrame, WebSocketFrame>)
        case notUpgraded(NIOAsyncChannel<HTTPServerRequestPart, HTTPPart<HTTPResponseHead, ByteBuffer>>)
    }

    public init(
        host: String,
        port: Int,
        username: String,
        password: String,
        eventLoopGroup: MultiThreadedEventLoopGroup
    ) {
        self.host = host
        self.port = port
        self.eventLoopGroup = eventLoopGroup
        self.username = username
        self.password = password
    }

    public func resetCredentialsState() {
        usernameReceived = false
        passwordReceived = false
    }

    public func reset() {
        resetCredentialsState()
        delay = nil
        refuse = false
        connectedClients = []
        pingHandler = nil
    }

    /// This method starts the server and handles incoming connections.
    public func run() async throws {
        let channel: NIOAsyncChannel<EventLoopFuture<UpgradeResult>, Never> = try await ServerBootstrap(group: eventLoopGroup)
            .serverChannelOption(ChannelOptions.socketOption(.so_reuseaddr), value: 1)
            .bind(host: host, port: port) { channel in
                channel.eventLoop.makeCompletedFuture {
                    let upgrader = NIOTypedWebSocketServerUpgrader<UpgradeResult>(
                        shouldUpgrade: { channel, _ in
                            if self.refuse {
                                print("Refusing connection upgrade \(channel.localAddress?.description ?? "")")
                                return channel.eventLoop.makeSucceededFuture(nil)
                            } else {
                                return channel.eventLoop.makeSucceededFuture(HTTPHeaders())
                            }
                        },
                        upgradePipelineHandler: { channel, _ in
                            channel.eventLoop.makeCompletedFuture {
                                let asyncChannel = try NIOAsyncChannel<WebSocketFrame, WebSocketFrame>(wrappingChannelSynchronously: channel)
                                return UpgradeResult.websocket(asyncChannel)
                            }
                        }
                    )

                    let serverUpgradeConfiguration = NIOTypedHTTPServerUpgradeConfiguration(
                        upgraders: [upgrader],
                        notUpgradingCompletionHandler: { channel in
                            channel.eventLoop.makeCompletedFuture {
                                try channel.pipeline.syncOperations.addHandler(HTTPByteBufferResponsePartHandler())
                                let asyncChannel = try NIOAsyncChannel<HTTPServerRequestPart, HTTPPart<HTTPResponseHead, ByteBuffer>>(wrappingChannelSynchronously: channel)
                                return UpgradeResult.notUpgraded(asyncChannel)
                            }
                        }
                    )

                    let negotiationResultFuture = try channel.pipeline.syncOperations.configureUpgradableHTTPServerPipeline(
                        configuration: .init(upgradeConfiguration: serverUpgradeConfiguration)
                    )

                    return negotiationResultFuture
                }
            }

        // We are handling each incoming connection in a separate child task. It is important
        // to use a discarding task group here which automatically discards finished child tasks.
        // A normal task group retains all child tasks and their outputs in memory until they are
        // consumed by iterating the group or by exiting the group. Since, we are never consuming
        // the results of the group we need the group to automatically discard them; otherwise, this
        // would result in a memory leak over time.
        try await withThrowingDiscardingTaskGroup { group in
            try await channel.executeThenClose { inbound in
                for try await upgradeResult in inbound {
                    group.addTask {
                        if let delay = self.delay {
                            try await Task.sleep(nanoseconds: .init(delay))
                        }
                        await self.handleUpgradeResult(upgradeResult)
                    }
                }
            }
        }
    }

    /// This method handles a single connection by echoing back all inbound data.
    private func handleUpgradeResult(_ upgradeResult: EventLoopFuture<UpgradeResult>) async {
        // Note that this method is non-throwing and we are catching any error.
        // We do this since we don't want to tear down the whole server when a single connection
        // encounters an error.
        do {
            switch try await upgradeResult.get() {
                case let .websocket(websocketChannel):
                    print("Handling websocket connection")
                    connectedClients.append(websocketChannel)
                    try await handleWebsocketChannel(websocketChannel)
                    print("Done handling websocket connection")
                case .notUpgraded:
                    print("Done handling HTTP connection")
            }
        } catch {
            print("Hit error: \(error)")
        }
    }

    private func handleWebsocketChannel(_ channel: NIOAsyncChannel<WebSocketFrame, WebSocketFrame>) async throws {
        try await channel.executeThenClose { inbound, outbound in
            try await withThrowingTaskGroup(of: Void.self) { group in
                group.addTask {
                    for try await frame in inbound {
                        switch frame.opcode {
                            case .ping:
                                print("Received ping")
                                self.pingHandler?()

                                var frameData = frame.data
                                let maskingKey = frame.maskKey

                                if let maskingKey {
                                    frameData.webSocketUnmask(maskingKey)
                                }

                                let responseFrame = WebSocketFrame(fin: true, opcode: .pong, data: frameData)
                                try await outbound.write(responseFrame)
                            case .connectionClose:
                                // This is an unsolicited close. We're going to send a response frame and
                                // then, when we've sent it, close up shop. We should send back the close code the remote
                                // peer sent us, unless they didn't send one at all.
                                print("Received close")
                                var data = frame.unmaskedData
                                let closeDataCode = data.readSlice(length: 2) ?? ByteBuffer()
                                let closeFrame = WebSocketFrame(fin: true, opcode: .connectionClose, data: closeDataCode)
                                try await outbound.write(closeFrame)
                                return
                            case .binary, .continuation, .pong:
                                // We ignore these frames.
                                break
                            case .text:
                                var frameData = frame.unmaskedData
                                let receivedText = frameData.readString(length: frameData.readableBytes)
                                print("Received text: \(receivedText ?? "nil")")
                                print("Username received: \(self.usernameReceived)")
                                print("Password received: \(self.passwordReceived)")
                                print("Instance: \(ObjectIdentifier(self))")

                                if !self.usernameReceived {
                                    self.usernameReceived = true
                                } else if !self.passwordReceived {
                                    let matchingPassword = receivedText == self.password
                                    if matchingPassword {
                                        print("Correct auth")
                                        self.passwordReceived = true
                                        var buffer = channel.channel.allocator.buffer(capacity: 16)
                                        buffer.writeString("authenticated")
                                        let frame = WebSocketFrame(fin: true, opcode: .text, data: buffer)
                                        try await outbound.write(frame)
                                    } else {
                                        print("Incorrect auth")
                                        self.usernameReceived = false
                                        var buffer = channel.channel.allocator.buffer(capacity: 32)
                                        buffer.writeString("err: Invalid credentials")
                                        let frame = WebSocketFrame(fin: true, opcode: .text, data: buffer)
                                        try await outbound.write(frame)
                                    }
                                }
                            default:
                                // Unknown frames are errors.
                                return
                        }
                    }
                }

                try await group.next()
                group.cancelAll()
            }
        }
    }

    public func send(message: String) {
        let buffer = ByteBuffer(string: message)
        let messageFrame = WebSocketFrame(fin: true, opcode: .text, data: buffer)

        // Send a message to all connected WebSocket clients
        for client in connectedClients {
            _ = client.channel.write(messageFrame)
        }
    }

    public func closeConnections() {
        for client in connectedClients {
            print("Closing connection \(client.channel.localAddress?.description ?? "")")
            var buffer = client.channel.allocator.buffer(capacity: 2)
            buffer.write(webSocketErrorCode: .normalClosure)

            let closeFrame = WebSocketFrame(fin: true, opcode: .connectionClose, data: buffer)
            client.channel.writeAndFlush(closeFrame).whenComplete { _ in
                client.channel.close(mode: .all).whenComplete { _ in
                    print("Channel closed.")
                }
            }
        }
        connectedClients = []
    }
}

final class HTTPByteBufferResponsePartHandler: ChannelOutboundHandler {
    typealias OutboundIn = HTTPPart<HTTPResponseHead, ByteBuffer>
    typealias OutboundOut = HTTPServerResponsePart

    func write(context: ChannelHandlerContext, data: NIOAny, promise: EventLoopPromise<Void>?) {
        let part = unwrapOutboundIn(data)
        switch part {
            case let .head(head):
                context.write(wrapOutboundOut(.head(head)), promise: promise)
            case let .body(buffer):
                context.write(wrapOutboundOut(.body(.byteBuffer(buffer))), promise: promise)
            case let .end(trailers):
                context.write(wrapOutboundOut(.end(trailers)), promise: promise)
        }
    }
}
