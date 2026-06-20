Yamir Indicators v3 - Fast Scalp Flush Reversal / Absorption
=============================================================

New study function:
  Fast Scalp Flush Reversal / Absorption

Existing studies remain in the same source file.

Recommended chart:
  Intraday chart, 5-second bars, Intraday Data Storage Time Unit = 1 Tick.

Fast defaults:
  Short window: 20 seconds
  Medium window: 60 seconds
  Long window: 120 seconds
  Early score begins after: 15 seconds
  Historical score display: 60 seconds
  Meaningful VI: 20 percent
  Minimum long flush: max(6 ticks, 8 basis points)
  Exclude first bar after a session gap from bid/ask flow: Yes

Display meanings:
  START  = not enough elapsed time for the early score.
  EARLY  = score is active, but the 2-minute window is not full yet.
  LIVE   = all three windows are full.
  An asterisk after a window label means that window is still partial.

Install:
  1. Back up the current Yamirindicators.cpp.
  2. Copy Yamirindicators_v3_fast.cpp to SierraChart\ACS_Source.
  3. Rename it Yamirindicators.cpp if desired.
  4. Analysis > Build Custom Studies DLL > select file > Remote Build.
  5. Remove only the older Multi-Horizon study from the target chart.
  6. Add Yamir Indicators > Fast Scalp Flush Reversal / Absorption.

Important:
  The new study does not remove or replace Volume Delta Indicator,
  Custom Pace of Tape Reader, or Custom Price Change Tape Reader.
