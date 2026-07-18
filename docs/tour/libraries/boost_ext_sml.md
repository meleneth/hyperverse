# Boost.Ext SML

Hyperverse uses Boost.Ext SML for small gameplay transition tables.

The local pattern is:

1. Store durable state in a public enum/model.
2. Recreate a private SML machine inside the update function.
3. Replay the durable phase into the machine.
4. Process one or more domain transition events.
5. Read the resulting SML state back into the durable enum.

This keeps the SML machine stateless between ticks and makes the saved component or subsystem model the source of truth.
