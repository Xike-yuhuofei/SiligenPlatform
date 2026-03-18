# HMI Architecture Proposal: Python + TCP Adapter

**Date:** 2026-01-21  
**Status:** Proposed  
**Context:** Adding a Simple HMI to the existing C++ Backend  

## 1. Requirement Analysis
The goal is to provide a "Simple HMI" for the backend system which currently operates via CLI. The system involves motion control (`motion`), dispensing (`dispensing`), and hardware drivers.

**Core Needs:**
*   **Real-time Feedback:** Essential for safety (monitoring motor currents, limit switches) and user confidence (operation confirmation).
*   **State Continuity:** The hardware connection must persist across multiple HMI commands.
*   **Simplicity:** Minimal code overhead, easy to maintain.

## 2. Architecture Comparison

We evaluated two primary approaches for the HMI interaction layer.

| Feature | Option A: CLI Wrapper (Subprocess) | Option B: TCP Adapter (Socket) |
| :--- | :--- | :--- |
| **Communication** | StdIn / StdOut Pipes | TCP Socket (JSON) |
| **Process Model** | Spawn-and-die OR Interactive Pipe | Daemon / Service |
| **Hardware State** | Lost on exit (unless interactive mode) | **Persisted** (Connection stays open) |
| **Real-time Push** | Difficult (Polling stdout) | **Native** (Async Push) |
| **Safety** | Low (Blind execution) | **High** (Instant feedback/Abort) |
| **Verdict** | Rejected for Control Systems | **Selected** |

**Decision:**
We select **Option B (Python + TCP)**.
Although the CLI wrapper appears simpler initially, it fails to provide the safety and state management required for motion control. The TCP approach offers a "Closed Loop" interaction model essential for industrial applications.

## 3. Selected Solution: TCP Adapter

We will implement a non-intrusive TCP Server Adapter in the Infrastructure layer. This allows the C++ backend to accept commands from a remote (or local) GUI without modifying the core Application logic.

### 3.1 Architecture Location
*   **Path:** `src/infrastructure/adapters/network/`
*   **Role:** Driving Adapter (Infrastructure Layer).
*   **Dependencies:** `Boost.Asio` (Network), `nlohmann/json` (Protocol).

### 3.2 Communication Protocol
We will use **JSON Lines (NDJSON)** over TCP. Each line is a complete JSON object ending with `\n`.

**Request (HMI -> C++):**
```json
{"cmd": "motion_move_abs", "params": {"axis": "x", "pos": 50.0}, "id": 101}
```

**Response (C++ -> HMI):**
```json
{"status": "ok", "id": 101, "data": {}}
```

**Async Push (C++ -> HMI):**
```json
{"event": "status_update", "data": {"x": 50.0, "y": 0.0, "state": "Idle"}}
```

### 3.3 C++ Class Design
1.  **`TcpServer`**: Manages the `boost::asio::io_context` and accepts incoming connections. Runs in a dedicated thread to ensure non-blocking hardware control.
2.  **`TcpSession`**: Handles the lifecycle of a single client connection. Reads lines, parses JSON, and writes responses.
3.  **`CommandDispatcher`**: Maps string commands (e.g., "stop") to specific Application UseCases. Acts as the boundary between Infrastructure and Application.

### 3.4 Python Client (HMI)
*   **Framework:** `tkinter` (Standard library) or `PyQt`.
*   **Logic:** A simple socket client class that connects on startup, sends JSON on button clicks, and runs a background thread to listen for status updates.

## 4. Implementation Roadmap

### Phase 1: Infrastructure Skeleton (Echo Test)
*   Create `src/infrastructure/adapters/network/TcpServer.{h,cpp}`.
*   Implement basic connection acceptance and text echo.
*   **Goal:** Verify `Boost.Asio` integration and network connectivity.

### Phase 2: Command Dispatching
*   Implement `CommandDispatcher`.
*   Connect `TcpSession` to the Dispatcher.
*   Map a simple UseCase (e.g., `GetSystemInfo`).
*   **Goal:** Verify JSON parsing and Application layer invocation.

### Phase 3: State Monitoring (Push)
*   Implement an observer mechanism in the `motion` domain.
*   TcpServer subscribes to motion updates and pushes JSON to connected clients.
*   **Goal:** Real-time visualization of machine coordinates.
