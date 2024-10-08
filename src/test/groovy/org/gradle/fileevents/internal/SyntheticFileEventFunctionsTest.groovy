package org.gradle.fileevents.internal

import org.gradle.fileevents.testfixtures.TestFileEventFunctions
import spock.lang.Specification

import java.util.concurrent.LinkedBlockingDeque
import java.util.concurrent.TimeUnit

class SyntheticFileEventFunctionsTest extends Specification {
    def service = new TestFileEventFunctions()
    def eventQueue = new LinkedBlockingDeque()
    def watcher = service
        .newWatcher(eventQueue)
        .start()

    def "normal termination produces termination event"() {
        when:
        watcher.shutdown()
        watcher.awaitTermination(1, TimeUnit.SECONDS)
        then:
        eventQueue*.toString() == ["TERMINATE"]
    }

    def "failure in run loop produces failure event followed by termination events"() {
        when:
        watcher.injectFailureIntoRunLoop()
        watcher.awaitTermination(1, TimeUnit.SECONDS)
        then:
        eventQueue*.toString() == ["FAILURE Error", "TERMINATE"]
    }
}
