# EventPP

Hyperverse wraps EventPP as `DomainEventBus`:

```cpp
using DomainEventBus = eventpp::EventQueue<DomainEventType, void(const DomainEvent&)>;
```

Systems enqueue domain facts during gameplay updates. Installed listeners process those facts and update persistent models or components.

Keep event payloads compact and factual. If a value must survive beyond event processing, store it in a component or subsystem model.
