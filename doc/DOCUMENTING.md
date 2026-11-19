# Writing a note

Write a note when a change adds a data path, a sensor mode, or a payload someone else must call or parse. Put it in `doc/<Name>.md`, PascalCase, next to [MotionService.md](MotionService.md). Update the note in the same change as the code.

Use these sections, in this order.

1. **Status.** A table: piece, state, file. State is `Written`, `Written, not called`, or `Planned`. A planned step names the function that will own it.
2. **Flow.** Draw data and calls apart. Title each diagram with what an arrow means, such as "a sample moves" or "a function calls the next one." Planned steps say so on the arrow. Under the data diagram, say where the bytes live after each move.
3. **Contract.** Types, constants, and arguments a caller must know. Table columns: Name, Kind, Meaning. One fact per row. Put the unit in the Meaning.
4. **Internals.** The same table, only for buffers and limits that change how a caller writes the call. Skip locals that do not.
5. **Timing.** Every interval, with the arithmetic.

Link the source file. Describe the behavior that is in the tree. A reader should be able to tell a written function from a planned call without opening the code.

[AccelFifo.md](AccelFifo.md) is an example.
