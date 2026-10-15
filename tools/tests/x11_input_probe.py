#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT
"""Send a key across Ieum's screen edge and observe it on an isolated test X display."""
import ctypes as c
import ctypes.util
import json
import sys
import time

x11 = c.CDLL(ctypes.util.find_library("X11"))
xtst = c.CDLL(ctypes.util.find_library("Xtst"))
Display = c.c_void_p
Window = c.c_ulong
x11.XOpenDisplay.argtypes = [c.c_char_p]
x11.XOpenDisplay.restype = Display
x11.XDefaultRootWindow.argtypes = [Display]
x11.XDefaultRootWindow.restype = Window
x11.XSelectInput.argtypes = [Display, Window, c.c_long]
x11.XSetInputFocus.argtypes = [Display, Window, c.c_int, c.c_ulong]
x11.XFlush.argtypes = [Display]
x11.XSync.argtypes = [Display, c.c_int]
x11.XPending.argtypes = [Display]
x11.XNextEvent.argtypes = [Display, c.c_void_p]
x11.XKeysymToKeycode.argtypes = [Display, c.c_ulong]
x11.XKeysymToKeycode.restype = c.c_uint
x11.XCloseDisplay.argtypes = [Display]
x11.XWarpPointer.argtypes = [Display, Window, Window, c.c_int, c.c_int, c.c_uint, c.c_uint, c.c_int, c.c_int]
x11.XQueryPointer.argtypes = [Display, Window, c.POINTER(Window), c.POINTER(Window), c.POINTER(c.c_int),
                             c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_uint)]
xtst.XTestFakeMotionEvent.argtypes = [Display, c.c_int, c.c_int, c.c_int, c.c_ulong]
xtst.XTestFakeRelativeMotionEvent.argtypes = [Display, c.c_int, c.c_int, c.c_ulong]
xtst.XTestFakeKeyEvent.argtypes = [Display, c.c_uint, c.c_int, c.c_ulong]

class XKeyEvent(c.Structure):
    _fields_ = [("type", c.c_int), ("serial", c.c_ulong), ("send_event", c.c_int), ("display", Display),
                ("window", Window), ("root", Window), ("subwindow", Window), ("time", c.c_ulong),
                ("x", c.c_int), ("y", c.c_int), ("x_root", c.c_int), ("y_root", c.c_int),
                ("state", c.c_uint), ("keycode", c.c_uint), ("same_screen", c.c_int)]

def pointer(display, root):
    root_return, child = Window(), Window()
    root_x, root_y, win_x, win_y = c.c_int(), c.c_int(), c.c_int(), c.c_int()
    mask = c.c_uint()
    x11.XQueryPointer(display, root, c.byref(root_return), c.byref(child), c.byref(root_x), c.byref(root_y),
                      c.byref(win_x), c.byref(win_y), c.byref(mask))
    return [root_x.value, root_y.value]

host = x11.XOpenDisplay(sys.argv[1].encode())
client = x11.XOpenDisplay(sys.argv[2].encode())
if not host or not client:
    raise RuntimeError("The owned test display could not be opened")
try:
    host_root, client_root = x11.XDefaultRootWindow(host), x11.XDefaultRootWindow(client)
    x11.XSelectInput(client, client_root, (1 << 0) | (1 << 1))
    x11.XSetInputFocus(client, client_root, 2, 0)
    x11.XWarpPointer(client, 0, client_root, 0, 0, 0, 0, 600, 300)
    x11.XSync(client, 0)
    before = pointer(client, client_root)
    xtst.XTestFakeMotionEvent(host, 0, 640, 400, 0)
    x11.XFlush(host)
    time.sleep(0.15)
    xtst.XTestFakeMotionEvent(host, 0, 1279, 400, 0)
    x11.XFlush(host)
    time.sleep(0.25)
    xtst.XTestFakeRelativeMotionEvent(host, 30, 12, 0)
    code = x11.XKeysymToKeycode(host, ord("a"))
    sent_at = time.monotonic()
    xtst.XTestFakeKeyEvent(host, code, 1, 0)
    xtst.XTestFakeKeyEvent(host, code, 0, 0)
    x11.XFlush(host)
    received = False
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        while x11.XPending(client):
            event = c.create_string_buffer(192)
            x11.XNextEvent(client, event)
            key = c.cast(event, c.POINTER(XKeyEvent)).contents
            if key.type == 2 and key.keycode == x11.XKeysymToKeycode(client, ord("a")):
                received = True
        if received:
            break
        time.sleep(0.02)
    after = pointer(client, client_root)
    result = {"keyReceived": received, "pointerMoved": after != before, "before": before, "after": after,
              "keyProbeMilliseconds": (time.monotonic() - sent_at) * 1000}
    print(json.dumps(result))
    raise SystemExit(0 if received and after != before else 1)
finally:
    x11.XCloseDisplay(host)
    x11.XCloseDisplay(client)
