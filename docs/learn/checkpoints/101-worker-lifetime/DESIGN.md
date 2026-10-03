# 101 Worker lifetime

- Reserve under one mutex before callable/thread construction; the cap counts tracked tasks, not every OS thread resource.
- Named std::thread separates construction rollback from detach failure; failed detach falls back to join with no second finish.
- Completion declared before local unique_ptr; callable/captures are destroyed before active decrements.
- Root uses a lambda capture moved to a local pointer; study uses a static entry with unique_ptr argument moved to a local pointer.
- Shared capture destruction releases its reference, not every other owner's object. Transferred socket ownership can outlive the job.
- Task-body exceptions are counted in study; root logs them. Construction failures are separate and restore reservation.
- notify under mutex prevents group destruction before final notify. Predicated wait handles missed/spurious notification.
- stop closes admission only; owner must arrange cancellation/wake, then drain before destroying borrowed state.
- No self-wait, no concurrent external API call surviving group destruction, no borrowed owner use from later TLS destructors.
- wait means task body/capture completion, not full thread exit. Join failures/mutex failures/destructor exceptions are fail-fast boundaries.
- Pure lifecycle test uses destructor gate, no sleeps to infer ordering;12 concurrent producers compete for2 blocked slots.
- worker_probe uses real3 loopback connections, three request routes, distinct result cells, actual RoundPlay tick0.
- Results/Runtime outlive group, and main reads results after drain mutex synchronization.
- Every launch creates a thread; this is not a pool. TLS/native thread cleanup and untracked asynchronous work are outside this group.
