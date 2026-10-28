# coproc_link (Phase 3)

RS485 link service above `core_protocol` + `hal_coproc`: request/response
matching, retries (500 ms timeout, 3 attempts, 250 ms backoff), async event
dispatch (JAM/EMPTY), error counters.
