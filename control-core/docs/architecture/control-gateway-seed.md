# control-gateway seed

This document defines the first migration slice for `modules/control-gateway`.

## Primary ownership

`control-gateway` owns:
- TCP-facing request/response adaptation for HMI and other programmatic callers
- TCP/JSON protocol parsing and response serialization
- request dispatch orchestration
- gateway-level error formatting and correlation ids

## Current canonical slice

The gateway slice now lands under:
- `modules/control-gateway/src/tcp/JsonProtocol.*`
- `modules/control-gateway/src/tcp/TcpCommandDispatcher.*`
- `modules/control-gateway/src/tcp/TcpSession.*`
- `modules/control-gateway/src/tcp/TcpServer.*`
- `modules/control-gateway/src/facades/tcp/TcpMotionFacade.*`
- `modules/control-gateway/src/facades/tcp/TcpDispensingFacade.*`
- `modules/control-gateway/src/facades/tcp/TcpRecipeFacade.*`
- `modules/control-gateway/src/facades/tcp/TcpSystemFacade.*`

## Keep out of control-gateway

Do not migrate directly:
- controller SDK wrappers
- file storage adapters
- task scheduler adapters
- recipe activation and process orchestration rules
- motion safety or trajectory domain rules

Gateway code may parse, validate, and dispatch, but should not decide device behavior or
business sequencing.

## Handler slimming rule

Every TCP handler should trend toward:
1. parse request
2. call process-core or motion-core application entry
3. serialize result

If a handler performs retries, polling, controller recovery, or long business sequencing,
that logic belongs elsewhere.

## New seed abstractions

The first seed headers added under `modules/control-gateway/include` intentionally define:
- a transport-neutral request envelope
- a transport-neutral response envelope
- a dispatch service abstraction
- TCP request views that can be reused by HMI-driven integrations

These headers are not yet wired into every production path. They provide the landing zone for future
protocol extraction.

## First real migrated slice

The following asset now has a real implementation under `modules/control-gateway`:
- `protocol/json_protocol.h/.cpp`

This gives the gateway module its first concrete protocol utility without yet moving the
large TCP dispatcher.
