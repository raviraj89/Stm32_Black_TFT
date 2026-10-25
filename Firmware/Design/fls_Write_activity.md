# `fls_Write()` activity diagram

```mermaid
flowchart TD
    A([Caller invokes fls_Write]) --> B{Data valid and range within flash?}
    B -- No --> X([Return E_NOT_OK])
    B -- Yes --> C{Worker started and idle?}
    C -- No --> X
    C -- Yes --> D[Save request and set status BUSY / PENDING]
    D --> E([Return E_OK to caller])
    D --> F[Flash worker starts request]
    F --> G{Bytes remaining?}
    G -- No --> Y([Set state to IDLE and job result to OK])

    G -- Yes --> H[Find sector containing current address]
    H --> I[Read full 4 KiB sector into RAM buffer]
    I --> J{Sector read succeeds?}
    J -- No --> K[Set result to E_NOT_OK]
    J -- Yes --> L[Copy caller data into matching buffer range]
    L --> M[Erase the sector and wait until ready]
    M --> N{Erase succeeds?}
    N -- No --> K
    N -- Yes --> O[Select next 256-byte page]
    O --> P{All sector pages processed?}
    P -- Yes --> Q[Advance to next sector in request]
    Q --> G
    P -- No --> R{Page contains any non-0xFF bytes?}
    R -- No --> S[Skip programming this erased page]
    R -- Yes --> T[Enable writes and program page-bounded chunks]
    T --> U{Programming succeeds?}
    U -- No --> K
    U -- Yes --> V[Advance to next page]
    S --> V
    V --> O

    K --> W([Set state to IDLE and job result to FAILED])
```

`fls_Write()` returns after the worker accepts the request. The source buffer
must remain valid until `fls_GetStatus()` reports `FLS_IDLE`; then inspect
`fls_GetJobResult()` to determine whether the write succeeded. Only one job can
be active at a time. The worker preserves bytes outside the requested range by
buffering and rewriting each affected sector. Power loss or an I/O failure
after erase can still lose data in that sector. The 4 KiB sector buffer is
static RAM.
