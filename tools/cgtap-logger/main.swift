// SPDX-FileCopyrightText: (C) 2026 Ieum Developers
// SPDX-License-Identifier: MIT

import CoreGraphics
import Foundation

private struct Record {
    let uptime: TimeInterval
    let eventTimestamp: UInt64
    let type: CGEventType
    let virtualKey: Int64
    let flags: UInt64
    let sourceState: Int64
    let keyboardType: Int64
}

private let capacity = 40
private let dumpKey: Int64 = 111 // F12
private var records: [Record] = []
private let recordsLock = NSLock()
private var eventTap: CFMachPort?

private func append(_ record: Record) {
    recordsLock.lock()
    records.append(record)
    if records.count > capacity {
        records.removeFirst(records.count - capacity)
    }
    recordsLock.unlock()
}

private func dumpRecords() {
    recordsLock.lock()
    let snapshot = records
    recordsLock.unlock()

    let currentUptime = ProcessInfo.processInfo.systemUptime
    let currentEpoch = Date().timeIntervalSince1970
    print("wall_epoch\ttype\tvkey\tflags\tevent_timestamp\tsource_state\tkeyboard_type")
    for record in snapshot {
        let wallEpoch = currentEpoch - (currentUptime - record.uptime)
        let typeName: String
        switch record.type {
        case .keyDown: typeName = "down"
        case .keyUp: typeName = "up"
        case .flagsChanged: typeName = "flags"
        default: typeName = "other"
        }
        print(
            String(format: "%.6f\t%@\t%lld\t0x%llx\t%llu\t%lld\t%lld",
                   wallEpoch, typeName, record.virtualKey, record.flags,
                   record.eventTimestamp, record.sourceState, record.keyboardType)
        )
    }
    fflush(stdout)
}

private func eventCallback(
    proxy: CGEventTapProxy,
    type: CGEventType,
    event: CGEvent,
    refcon: UnsafeMutableRawPointer?
) -> Unmanaged<CGEvent>? {
    if type == .tapDisabledByTimeout || type == .tapDisabledByUserInput {
        if let eventTap {
            CGEvent.tapEnable(tap: eventTap, enable: true)
        }
        return Unmanaged.passUnretained(event)
    }

    let virtualKey = event.getIntegerValueField(.keyboardEventKeycode)
    append(Record(
        uptime: ProcessInfo.processInfo.systemUptime,
        eventTimestamp: event.timestamp,
        type: type,
        virtualKey: virtualKey,
        flags: event.flags.rawValue,
        sourceState: event.getIntegerValueField(.eventSourceStateID),
        keyboardType: event.getIntegerValueField(.keyboardEventKeyboardType)
    ))
    if type == .keyDown && virtualKey == dumpKey {
        dumpRecords()
    }
    return Unmanaged.passUnretained(event)
}

let mask = (CGEventMask(1) << CGEventType.keyDown.rawValue)
    | (CGEventMask(1) << CGEventType.keyUp.rawValue)
    | (CGEventMask(1) << CGEventType.flagsChanged.rawValue)

guard let tap = CGEvent.tapCreate(
    tap: .cgSessionEventTap,
    place: .headInsertEventTap,
    options: .listenOnly,
    eventsOfInterest: mask,
    callback: eventCallback,
    userInfo: nil
) else {
    fputs("Unable to create CGEvent tap. Grant Input Monitoring permission.\n", stderr)
    exit(1)
}

eventTap = tap
let source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0)
CFRunLoopAddSource(CFRunLoopGetCurrent(), source, .commonModes)
CGEvent.tapEnable(tap: tap, enable: true)
fputs("Ieum CGEvent logger active. Press F12 to dump the last 40 events.\n", stderr)
CFRunLoopRun()
