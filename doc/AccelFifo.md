# Accelerometer FIFO

The BMA421 records samples into its FIFO. `DrainFifo` copies them out once 150 bytes are waiting. Source: [src/drivers/Bma421.cpp](../src/drivers/Bma421.cpp).

Gesture wake still uses one fresh sample from `Process`. Tap wake uses the touch controller and does not read this sensor.

## Status

| Piece | State | Where |
|---|---|---|
| 25 Hz, FIFO enabled, 150-byte watermark | Written | `Bma421::Init` |
| `ToWatchAxes` | Written | `Bma421` |
| `DrainFifo` | Written, not called | `Bma421::DrainFifo` |
| Call `DrainFifo` every 100 ms | Planned | `SystemTask::UpdateMotion` |
| Forward the batch without touching gesture history | Planned | `MotionController` |
| Send the batch, then drop the local array | Planned | `MotionService` |

`MotionService` does not see the chip. The notification layout is a later note.

## Flow

An arrow in the first diagram is a sample moving from one place to the next. An arrow in the second diagram is one function calling another. The two are separate, so a line never means both at once.

### Where a sample moves

While the FIFO holds fewer than 150 bytes, the sample stays in the FIFO. That wait is not an arrow. The arrow appears when `DrainFifo` copies the batch out.

```mermaid
flowchart TD
  chip[BMA421 produces a sample at 25 Hz]
  chip -->|writes each sample| fifo[FIFO. The sample waits here until 150 bytes are stored]
  chip -->|writes the latest sample| regs[Current-sample registers]

  regs -->|Process reads one sample| axes[ToWatchAxes]
  axes -->|one Sample| gestures[MotionController gesture history]
  gestures -->|6 bytes, when that characteristic is subscribed| six[Existing motion characteristic]

  fifo -->|DrainFifo copies the batch| local[Caller array of Samples]
  local -->|planned handoff, then the array is discarded| notify[MotionService notification]
```

After the copy, the chip no longer has those samples. They live in the caller’s array until the planned send, and the watch does not keep another copy. Samples that arrive afterward go back into the FIFO and wait for the next time the line is reached.

### Which function calls the next

```mermaid
flowchart TD
  tick[UpdateMotion, every 100 ms]
  tick -->|calls today| process[Process]
  process -->|calls today| axes[ToWatchAxes]
  axes -->|calls today| update[MotionController Update]
  update -->|calls today, when subscribed| six[OnNewMotionValues]

  tick -->|planned call| drain[DrainFifo]
  drain -->|planned call, when the copy is not empty| forward[MotionController batch method]
  forward -->|planned call| send[MotionService burst notify]
```

## Contract

| Name | Kind | Meaning |
|---|---|---|
| `Sample` | Struct, 6 bytes | One reading in watch axes. `x`, `y`, `z` are `int16`. 1024 counts is 1 g. `x` is the chip's Y and `y` is the chip's X. |
| `Values` | Existing struct | One live reading: `steps` plus axes copied from a `Sample`. |
| `rawX`, `rawY`, `rawZ` | Arguments | Chip axis order, before the swap in `ToWatchAxes`. |
| `scale` | Lookup | Counts per g for the active range. At ±2 g this is 1024, so the stored count equals the raw count. |
| `accel_conf.odr` | Sensor setting | `BMA4_OUTPUT_DATA_RATE_25HZ`. |
| `fifoWatermarkBytes` | Constant, 150 | `DrainFifo` returns 0 until the FIFO holds at least this many bytes. |
| `samples` | Caller array | Destination for drained frames. The caller owns it. |
| `capacity` | Count | Slots in `samples`. A return equal to `capacity` means frames may still be waiting. Pass at least 25. |
| `available` | Byte count | FIFO fill level at the start of the call. |

`Init` also selects headerless accelerometer frames, filtered data, no extra downsampling, and stop-on-full. Stop-on-full keeps a late drain from wrapping the 1 KB buffer. Averaging mode (`BMA4_CIC_AVG_MODE`) is unchanged.

## Internals

| Name | Kind | Meaning |
|---|---|---|
| `fifoReadChunkBytes` | Constant, 96 | Largest read in one transfer: 16 frames. The vendor read copies the request onto the stack. The system task stack is 350 words. |
| `chunk` | Byte count | This transfer. Capped by 96, by the remaining capacity, and rounded down to a multiple of 6. |
| `raw` | 96-byte buffer | Unparsed bytes for one chunk. |
| `frame` | Bosch descriptor | Points at `raw` for `bma4_read_fifo_data`. |
| `parsed` | Chip frames | Up to 16 triples in chip axis order, before `ToWatchAxes`. |
| `written` | Count | Frames stored in this call. This is the return value. |

## Timing

The task checks every 100 ms. The FIFO is drained when it reaches 150 bytes. A frame is 6 bytes, so that is 25 samples. At 25 Hz, 25 samples is 1 second. The burst leaves within one 100 ms check of crossing the watermark.
