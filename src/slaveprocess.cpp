/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#include "slaveprocess.hpp"

using namespace juce;

namespace element {

enum
{
    magicMastSlaveConnectionHeader = 0x712baf04
};

static const char* startMessage = "__ipc_st";
static const char* killMessage = "__ipc_k_";
static const char* pingMessage = "__ipc_p_";
enum
{
    specialMessageSize = 8,
    defaultTimeoutMs = 8000
};

static String getCommandLinePrefix (const String& commandLineUniqueID)
{
    return "--" + commandLineUniqueID + ":";
}

//==============================================================================
// This thread sends and receives ping messages every second, so that it
// can find out if the other process has stopped running.
struct ChildProcessPingThread : public Thread,
                                private AsyncUpdater
{
    ChildProcessPingThread (int timeout) : Thread ("IPC ping"), timeoutMs (timeout)
    {
        pingReceived();
    }

    static bool isPingMessage (const MemoryBlock& m) noexcept
    {
        return memcmp (m.getData(), pingMessage, specialMessageSize) == 0;
    }

    void pingReceived() noexcept { countdown = timeoutMs / 1000 + 1; }
    void triggerConnectionLostMessage() { triggerAsyncUpdate(); }

    virtual bool sendPingMessage (const MemoryBlock&) = 0;
    virtual void pingFailed() = 0;

    int timeoutMs;

private:
    Atomic<int> countdown;

    void handleAsyncUpdate() override { pingFailed(); }

    void run() override
    {
        while (! threadShouldExit())
        {
            if (--countdown <= 0 || ! sendPingMessage (MemoryBlock (pingMessage, specialMessageSize)))
            {
                triggerConnectionLostMessage();
                break;
            }

            wait (1000);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChildProcessPingThread)
};

//==============================================================================
struct ChildProcessMaster::Connection : public InterprocessConnection,
                                        private ChildProcessPingThread
{
    Connection (ChildProcessMaster& m, const String& pipeName, int timeout)
        : InterprocessConnection (false, magicMastSlaveConnectionHeader),
          ChildProcessPingThread (timeout),
          owner (m)
    {
        if (createPipe (pipeName, timeoutMs))
            startThread (4);
    }

    ~Connection()
    {
        int timeout = 10000;
        while (isThreadRunning() && timeout >= 0)
        {
            signalThreadShouldExit();
            notify();
            if (waitForThreadToExit (500))
                break;
        }

        if (isThreadRunning())
            stopThread (0);
    }

private:
    void connectionMade() override {}
    void connectionLost() override { owner.handleConnectionLost(); }

    bool sendPingMessage (const MemoryBlock& m) override { return owner.sendMessageToSlave (m); }
    void pingFailed() override { connectionLost(); }

    void messageReceived (const MemoryBlock& m) override
    {
        pingReceived();

        if (m.getSize() != specialMessageSize || ! isPingMessage (m))
            owner.handleMessageFromSlave (m);
    }

    ChildProcessMaster& owner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Connection)
};

//==============================================================================
ChildProcessMaster::ChildProcessMaster() {}

ChildProcessMaster::~ChildProcessMaster()
{
    if (connection != nullptr)
    {
        if (childProcess.isRunning())
            sendMessageToSlave (MemoryBlock (killMessage, specialMessageSize));
        connection->disconnect();
        connection = nullptr;
    }

    if (childProcess.isRunning())
        childProcess.kill();
}

void ChildProcessMaster::handleConnectionLost() {}

bool ChildProcessMaster::sendMessageToSlave (const MemoryBlock& mb)
{
    if (childProcess.isRunning() && connection != nullptr)
        return connection->sendMessage (mb);
    return false;
}

bool ChildProcessMaster::launchSlaveProcess (const File& executable, const String& commandLineUniqueID, int timeoutMs, int streamFlags)
{
    connection = nullptr;
    if (childProcess.isRunning())
        childProcess.kill();
    jassert (! childProcess.isRunning());
    const String pipeName ("p" + String::toHexString (Random().nextInt64()));

    StringArray args;
    args.add (executable.getFullPathName());
    args.add (getCommandLinePrefix (commandLineUniqueID) + pipeName);

    if (childProcess.start (args, streamFlags))
    {
        connection = std::make_unique<Connection> (*this, pipeName, timeoutMs <= 0 ? defaultTimeoutMs : timeoutMs);

        if (connection->isConnected())
        {
            sendMessageToSlave (MemoryBlock (startMessage, specialMessageSize));
            return true;
        }

        connection = nullptr;
    }

    return false;
}

//==============================================================================
struct ChildProcessSlave::Connection : public InterprocessConnection,
                                       private ChildProcessPingThread
{
    Connection (ChildProcessSlave& p, const String& pipeName, int timeout)
        : InterprocessConnection (false, magicMastSlaveConnectionHeader),
          ChildProcessPingThread (timeout),
          owner (p)
    {
        connectToPipe (pipeName, timeoutMs);
        startThread (4);
    }

    ~Connection()
    {
        stopThread (10000);
    }

private:
    ChildProcessSlave& owner;

    void connectionMade() override {}
    void connectionLost() override { owner.handleConnectionLost(); }

    bool sendPingMessage (const MemoryBlock& m) override { return owner.sendMessageToMaster (m); }
    void pingFailed() override { connectionLost(); }

    void messageReceived (const MemoryBlock& m) override
    {
        pingReceived();

        if (m.getSize() == specialMessageSize)
        {
            if (isPingMessage (m))
                return;

            if (memcmp (m.getData(), killMessage, specialMessageSize) == 0)
            {
                triggerConnectionLostMessage();
                return;
            }

            if (memcmp (m.getData(), startMessage, specialMessageSize) == 0)
            {
                owner.handleConnectionMade();
                return;
            }
        }

        owner.handleMessageFromMaster (m);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Connection)
};

//==============================================================================
ChildProcessSlave::ChildProcessSlave() {}
ChildProcessSlave::~ChildProcessSlave() {}

void ChildProcessSlave::handleConnectionMade() {}
void ChildProcessSlave::handleConnectionLost() {}

bool ChildProcessSlave::sendMessageToMaster (const MemoryBlock& mb)
{
    if (connection != nullptr)
        return connection->sendMessage (mb);

    jassertfalse; // this can only be used when the connection is active!
    return false;
}

bool ChildProcessSlave::initialiseFromCommandLine (const String& commandLine,
                                                   const String& commandLineUniqueID,
                                                   int timeoutMs)
{
    String prefix (getCommandLinePrefix (commandLineUniqueID));

    if (commandLine.trim().startsWith (prefix))
    {
        String pipeName (commandLine.fromFirstOccurrenceOf (prefix, false, false)
                             .upToFirstOccurrenceOf (" ", false, false)
                             .trim());

        if (pipeName.isNotEmpty())
        {
            connection.reset (new Connection (*this, pipeName, timeoutMs <= 0 ? defaultTimeoutMs : timeoutMs));

            if (! connection->isConnected())
                connection = nullptr;
        }
    }

    return connection != nullptr;
}

} // namespace element
