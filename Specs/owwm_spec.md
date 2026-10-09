# OpenWindows Window Manager (owwm) Architecture Specification

## 1. Overview
The `owwm` subsystem provides a lightweight, zero-allocation compositing window manager operating over Cairo (`cairo64.owd`) and `userinterface64.owd`.

## 2. Window Hierarchy & Frame Decorations
- Window frames feature an active titlebar, close button, minimize button, and maximize button.
- Z-order is managed through a static double-linked table with a fixed limit (e.g. 32 top-level windows).
- Dirty-rectangle clipping bounds ensure that only modified visual regions are blitted to the primary framebuffer.

## 3. Input & Mouse Dispatch
- PS/2 mouse motion and button events from `ps2hid.owc` are intercepted and dispatched to window focus bounds.
- Cursor is rendered using hardware cursor registers if supported, or via software alpha-blended sprite compositing.
