# Development

## Local Docs Site

From `docs/tour`:

```sh
npm ci
npm run dev
```

Build the published site with:

```sh
npm run build
```

## Generated API Docs

The repository also includes the same local Doxygen entry point used by Fairlanes:

```sh
./make_docs.sh
```

Generated API docs are written to `docs/html`.

## Documentation Expectations

When adding or changing gameplay architecture:

- Update [Event Reference](./event-reference.md) for new `DomainEventType` values.
- Update [Context Objects](./context-objects.md) for new authority-bearing contexts.
- Update [State Machines](./state-machines.md) when adding a phase enum or SML transition table.
- Keep docs tied to source names so stale terminology is easy to grep.
