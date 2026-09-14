# Architecture — Build Standards

## Layering (L0 → L1 → L2)

The system is decomposed into a layered stack with strict one-way dependency:

- **L0 — Foundation / Platform:** Shared infrastructure the project configures and
  uses but does not implement. SDKs, managed services, RTOS, WiFi/BLE/USB stacks.
- **L1 — Interfaces:** External-facing modules the project owns — protocol decoders,
  bus drivers, API handlers, adapters. Each talks to a black-box peer.
- **L2 — Application logic:** Decision functions. Consume L1 output, decide, drive
  interfaces. No wire-format parsing.

### Rules

- One module per component. An interface used by exactly one feature still gets its
  own module, so it can be tested, replaced, or reused independently.
- Lower layers must never depend on higher ones. Use dependency inversion: a lower
  module exposes a callback hook; the composition root (main) wires it up.
- Extract the pure core of each interface/logic as a free function — separable from
  I/O so it's testable on the host tier.

## Source layout

```text
firmware/
├── main/          # composition root, app_main
├── components/    # one directory per L1/L2 module
│   ├── wifi_manager/
│   ├── ble_hid/
│   └── hid_translator/
└── tests/         # host tests live alongside, or at repo root
tests/
├── host/          # pure logic tests (automated)
├── target/        # target integration tests (your observation)
└── bench/          # manual/physical-action tests (you perform)
```

## Naming conventions

- Components: `snake_case` (e.g., `wifi_manager`, `ble_hid`)
- Tests: `test_<area>_<nn>` (e.g., `test_wifi_connect`, `test_layout_de`)
- Log tags: `ESP_LOGI(TAG, ...)` with `TAG` = module name

## Build configuration

- `sdkconfig.defaults` — persistent config (flash size, partition scheme).
- `partitions.csv` — partition table (adjust for OTA if needed).
- Version comes from `PROJECT_VER` (= git tag or `git describe` output).
