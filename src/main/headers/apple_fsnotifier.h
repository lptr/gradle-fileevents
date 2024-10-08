#pragma once

#if defined(__APPLE__)

#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <variant>

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>

#include "generic_fsnotifier.h"
#include "org_gradle_fileevents_internal_OsxFileEventFunctions.h"

using namespace std;

template <typename T>
class BlockingQueue {
private:
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<T> queue;

public:
    // Enqueue an item into the queue and notify one waiting thread
    void enqueue(const T& item) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            queue.push(item);
        }
        cv.notify_one();
    }

    // Dequeue an item from the queue. Blocks if the queue is empty until an item is available.
    T dequeue() {
        std::unique_lock<std::mutex> lock(mtx);
        // Block until the queue isn't empty
        cv.wait(lock, [this] { return !queue.empty(); });
        T item = queue.front();
        queue.pop();
        return item;
    }

    size_t size() {
        return queue.size();
    }
};

class Server;

static void handleEventsCallback(
    ConstFSEventStreamRef streamRef,
    void* clientCallBackInfo,
    size_t numEvents,
    void* eventPaths,
    const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId*);

class WatchPoint {
public:
    WatchPoint(Server* server, dispatch_queue_t dispatchQueue, const u16string& path, long latencyInMillis);
    ~WatchPoint();

private:
    FSEventStreamRef watcherStream;
};

struct FileEvent {
    std::string eventPath;
    FSEventStreamEventFlags eventFlags;
    FSEventStreamEventId eventId;
};

struct ErrorEvent {
    std::string message;
};

struct PoisonPill { };

using QueueItem = std::variant<FileEvent, ErrorEvent, PoisonPill>;

class Server : public AbstractServer {
public:
    Server(JNIEnv* env, jobject watcherCallback, long latencyInMillis);
    virtual ~Server();

    virtual void registerPaths(const vector<u16string>& paths) override;
    virtual bool unregisterPaths(const vector<u16string>& paths) override;

protected:
    void initializeRunLoop() override;
    void runLoop() override;

    void shutdownRunLoop() override;

private:
    void handleEvent(JNIEnv* env, const char* path, FSEventStreamEventFlags flags, FSEventStreamEventId eventId);
    void handleEvents(
        size_t numEvents,
        char** eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[]);

    friend void handleEventsCallback(
        ConstFSEventStreamRef stream,
        void* clientCallBackInfo,
        size_t numEvents,
        void* eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[]);

    const long latencyInMillis;
    recursive_mutex mutationMutex;
    unordered_map<u16string, WatchPoint> watchPoints;

    const dispatch_queue_t dispatchQueue;
    BlockingQueue<QueueItem> eventQueue;
};

#endif
