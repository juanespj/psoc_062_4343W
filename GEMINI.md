# Project Context: PSoC 62 4343W Zephyr Project

This is a Zephyr RTOS project targeting the **Infineon/Cypress CY8CPROTO-062-4343W** prototyping kit.

## Architecture & Hardware
- **Board:** `cy8cproto_062_4343w`
- **SoC:** PSoC 62 (Dual-core ARM Cortex-M4 and Cortex-M0+, though typically targeting the M4).
- **Connectivity:** 4343W WiFi/Bluetooth combo chip.
- **Key Drivers:** Uses the `hal_infineon` module (CAT1 family).

## Workspace Configuration
- **Type:** Workspace Application (T2).
- **Manifest:** `west.yaml` is located in the project root.
- **Build System:** Zephyr/CMake/Ninja.

## Build & Flash Commands
- **Standard Build:** `west build -b cy8cproto_062_4343w`
- **Clean Build:** `west build -p always -b cy8cproto_062_4343w`
- **Flash:** `west flash` (Requires OpenOCD or ModusToolbox programming tools installed).

## Known Configurations & Constraints
- **UART Async API:** Enabled in `prj.conf` via `CONFIG_UART_ASYNC_API=y`.
- **DMA:** Uses Infineon-specific DMA. Correct Kconfig symbol is `CONFIG_USE_INFINEON_DMA=y`.
- **DeviceTree:** Custom overlays for UART/DMA should be placed in `app.overlay`. Note that `uart5` is often used for the KitProg3 bridge/console.

## Development Standards
- Follow Zephyr coding style and Devicetree conventions.
- Use `west` for all build and workspace management tasks.
- Always verify Kconfig symbols against the active Zephyr version (currently tracking `v3.7.0` in `west.yaml`).
