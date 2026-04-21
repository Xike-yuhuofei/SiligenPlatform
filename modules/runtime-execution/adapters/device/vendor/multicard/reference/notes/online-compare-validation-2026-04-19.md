# Online compare validation - 2026-04-19

This note records repository-local real-board evidence gathered against the checked-in
`MultiCard.dll` / `MultiCard.lib` pair after the board at `192.168.0.1` was powered back on.

Scope:

- Use a no-motion probe against the real board.
- Keep compare outputs detached with `MC_CmpBufSetChannel(0, 0)`.
- Validate `MC_Open`, `MC_CmpBufSts`, `MC_CmpBufData`, and a follow-up refill path.

Observed results:

- `MC_Open(1, "192.168.0.200", 0, "192.168.0.1", 0)` succeeded.
- Baseline `MC_CmpBufSts` returned `status=0`, `remain1=0`, `remain2=0`, `space1=256`, `space2=256`.
- First `MC_CmpBufData` load with `pBuf1.len=2` and `pBuf2.len=2` succeeded.
- Post-load `MC_CmpBufSts` returned `status=3`, `remain1=2`, `remain2=2`, `space1=254`, `space2=254`.
- Second `MC_CmpBufData` refill with `pBuf1.len=1` and `pBuf2.len=1` succeeded.
- Post-refill `MC_CmpBufSts` returned `status=3`, `remain1=3`, `remain2=3`, `space1=253`, `space2=253`.

Conclusions:

- The real board accepts dual-buffer compare loads through `MC_CmpBufData`.
- The real board also accepts a follow-up refill while data remains queued.
- The current runtime path that fills `pBuf1` only and performs one preload is a conservative
  implementation choice, not a proven hardware limit.

Still not proven by this probe:

- Trigger behavior while axes are moving.
- Multi-source compare coordination beyond one selected compare source axis.
- `MC_CmpRpt` / `MC_CmpBufRpt`, because the checked-in header and binary pair still drift.
