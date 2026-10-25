# `fls_Write()` activity diagram

```mermaid
flowchart TD
    A([Start]) --> B{Data valid and range within flash?}
    B -- No --> X([Return E_NOT_OK])
    B -- Yes --> C{Begin job succeeds?}
    C -- No --> X
    C -- Yes --> D{Bytes remaining?}
    D -- No --> Y[Set state to IDLE and job result to OK]
    Y --> Z([Return E_OK])

    D -- Yes --> E[Find sector containing current address]
    E --> F[Read full 4 KiB sector into RAM buffer]
    F --> G{Sector read succeeds?}
    G -- No --> H[Set result to E_NOT_OK]
    G -- Yes --> I[Copy caller data into matching buffer range]
    I --> J[Erase the sector and wait until ready]
    J --> K{Erase succeeds?}
    K -- No --> H
    K -- Yes --> L[Select next 256-byte page]
    L --> M{All sector pages processed?}
    M -- Yes --> N[Advance to next sector in request]
    N --> D
    M -- No --> O{Page contains any non-0xFF bytes?}
    O -- No --> P[Skip programming this erased page]
    O -- Yes --> Q[Enable writes and program page-bounded chunks]
    Q --> R{Programming succeeds?}
    R -- No --> H
    R -- Yes --> S[Advance to next page]
    P --> S
    S --> L

    H --> T[Set state to IDLE and job result to FAILED]
    T --> U([Return E_NOT_OK])
```

`fls_Write()` preserves bytes outside the requested range by buffering and
rewriting each affected sector. The sector is erased before it is restored, so
power loss or an I/O failure during that interval can still lose data in that
sector. The 4 KiB sector buffer is static RAM.
