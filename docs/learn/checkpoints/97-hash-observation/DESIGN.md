# 97 Consistent samples and bounded comparisons

- Separate seq_cst atomic fields can form a logically mixed pair without a data race.
- One mutex protects local/remote windows, latest diagnostic value, and the comparison frontier.
- latest_remote copies one accepted arrival nondestructively; it is not complete history or maximum tick.
- Periodic windows start at Period, use optional for presence, reject conflict/too_far, and never evict pending samples.
- poll consumes only the earliest complete pair, including mismatches. Copies leave the lock before logging/comparing.
- Cursor is u64, input ticks u32; exhaustion never wraps to zero.
- clear resets retained storage atomically, not external messages or round identity.
- Root template matches this body; namespace and HashExchange<600,8> alias differ.
- Root Session records both origins, main polls all ready pairs. Invalid/conflicting/out-of-window records cause failure; already consumed remote ticks are ignored.
- Current wire has no round identity or hash algorithm negotiation; those remain separate boundaries.
