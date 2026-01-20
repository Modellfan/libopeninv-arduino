# libopeninv-arduino Example Standards

This document defines naming and structure standards for example sketches in this repository.

## Goals
- Keep examples consistent and easy to compare.
- Make CAN callbacks and filter setup predictable.
- Avoid extra dispatcher classes in simple examples.

## Function Naming
Match the callback names used in `examples/canhardware_test/src/main.cpp`:
- `Can1Callback`, `Can2Callback`, `Can3Callback` for CAN receive handlers.
- `SetCanFilters` for registering user CAN messages.

If an example uses only one bus, still use `Can1Callback` and `SetCanFilters`.

## Structure
Follow the `examples/canopen_basic/canopen_basic.ino` layout:
- `setup()` initializes serial, parameters, CAN, and registers callbacks.
- `loop()` calls `Poll()` and performs periodic work.
- Use a single callback (FunctionPointerCallback) for CAN RX handling.

## Parameters
- Parameter definitions live in `include/param_prj.h`.
- Use `Param::SetFloat` from module code (e.g., `isa_shunt.cpp`) to update values.
- Use `Param::GetFloat` in example printouts.
