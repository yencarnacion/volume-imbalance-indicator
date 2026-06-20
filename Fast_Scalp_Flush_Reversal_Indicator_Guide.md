# Fast Scalp Flush Reversal / Absorption Indicator

## Setup, display, and interpretation guide

This guide covers the Sierra Chart ACSIL study named:

> **Fast Scalp Flush Reversal / Absorption**

It is designed for fast discretionary equity scalping after sharp directional moves, especially downward flushes that may be exhausting and beginning to reverse.

The study does **not** replace your existing indicators. Keep using your separate:

- Volume Delta Indicator
- Custom Price Change Tape Reader
- Custom Pace of Tape Reader

The new study combines short-term order-flow, price response, and pace information into one compact panel while still displaying the raw values.

---

# 1. What the indicator measures

The study uses three rolling, time-based windows:

| Window | Default | Purpose |
|---|---:|---|
| Short | 20 seconds | Detects the newest change first |
| Medium | 60 seconds | Shows whether the move is spreading beyond the very short term |
| Long | 120 seconds | Provides flush and trend context |

For each window, it calculates:

- Ask volume
- Bid volume
- Volume delta
- Volume imbalance percentage
- Price change
- Trade count
- Recent high and low behavior

It also compares the current 20-second trade rate with the previous 20-second period to estimate **pace acceleration**.

The central idea is not simply “more buyers than sellers.” The study looks for situations such as:

- Aggressive selling remains heavy, but price stops falling efficiently.
- Delta remains negative, but price holds or begins to rebound.
- The 20-second readings improve before the 1-minute and 2-minute readings.
- Trading activity remains high while price response changes direction.

That relationship can indicate absorption or a possible reversal before raw delta becomes positive.

---

# 2. Correct chart setup

## Recommended chart type

Use the study on a separate **5-second intraday chart**.

The study is time-based, so it can also run on other intraday bar periods, but the 5-second chart provides a good balance between responsiveness and stability.

## Intraday data storage

In Sierra Chart:

1. Open **Global Settings**.
2. Open **Data/Trade Service Settings** or the applicable data settings window.
3. Confirm that **Intraday Data Storage Time Unit** is set to:

```text
1 Tick
```

This helps preserve the bid-volume and ask-volume classification used by `SC_BIDVOL` and `SC_ASKVOL`.

## Chart update speed

For fast live updates:

1. Open the 5-second chart.
2. Select **Chart → Chart Settings**.
3. Open the **Display** or update-related tab.
4. Set **Chart Update Interval in Milliseconds** to approximately:

```text
100
```

A lower number updates more frequently but uses more CPU. Start at 100 milliseconds and only reduce it if the computer remains stable.

Also verify that any setting named similar to:

```text
Minimum Chart Update Interval for ACSIL UpdateAlways
```

is not set higher than the chart update interval.

## Amount of data loaded

The live calculation only needs the most recent two minutes, plus a small amount of recent display history. With the default settings, the detailed calculation is limited to roughly the recent three minutes.

You do not need to load weeks of data for this chart.

A practical setting is:

1. Select **Chart → Chart Settings**.
2. Open **Data Limiting**.
3. Set **Load Data Limiting Method** to **Use Number of Days to Load**.
4. Set **Days to Load for Intraday Chart Data Type** to:

```text
1
```

Sierra Chart normally limits chart loading by day or date, not by an exact number of minutes. The study itself avoids doing detailed rolling calculations on unnecessary older bars.

---

# 3. Installing and compiling the study

## Copy the source file

1. Back up your current source file:

```text
C:\SierraChart\ACS_Source\Yamirindicators.cpp
```

2. Copy the new fast-study source into:

```text
C:\SierraChart\ACS_Source\
```

3. Rename it to:

```text
Yamirindicators.cpp
```

Replacing the old source is acceptable after making a backup because the new file preserves the existing indicators and adds the new fast study.

## Compile the DLL

1. In Sierra Chart, select:

```text
Analysis → Build Custom Studies DLL
```

2. Select `Yamirindicators.cpp`.
3. Select:

```text
Build → Remote Build
```

4. Wait for the build to complete successfully.
5. Close the build window.

---

# 4. Adding the correct study

Use the newer study, not the older 1-minute/3-minute/5-minute version.

1. Click the bottom-right 5-second chart to make it active.
2. Select:

```text
Analysis → Studies
```

3. In **Studies to Graph**, remove the older study named:

```text
Multi-Horizon Flush Reversal / Absorption
```

4. Click **Add Custom Study**.
5. Expand **Yamir Indicators**.
6. Select:

```text
Fast Scalp Flush Reversal / Absorption
```

7. Click **Add**.
8. Highlight the newly added study.
9. Click **Settings**.

You have the correct version when its inputs begin with:

```text
Short Window Seconds                  20
Medium Window Seconds                 60
Long Window Seconds                  120
Minimum Seconds Before Early Score    15
```

The older version used 60, 180, and 300 seconds and took much longer to become fully ready.

---

# 5. Recommended Settings and Inputs

Open:

```text
Analysis → Studies
→ Fast Scalp Flush Reversal / Absorption
→ Settings
→ Settings and Inputs
```

Use these initial values:

| Input | Recommended value |
|---|---:|
| Short Window Seconds | 20 |
| Medium Window Seconds | 60 |
| Long Window Seconds | 120 |
| Minimum Seconds Before Early Score | 15 |
| Minimum Bid+Ask Volume In Full Short Window | 1000 |
| Minimum Trades In Full Short Window | 15 |
| Meaningful VI Percent | 20 |
| Minimum Long-Window Flush Ticks | 6 |
| Minimum Long-Window Flush Basis Points | 8 |
| Flush High/Low Recency Seconds | 45 |
| Absorption State Threshold | 35 |
| Strong Reversal Threshold | 60 |
| Exclude First Bar After Session Gap From Flow | Yes |
| Session Gap Detection Seconds | 600 |
| Historical Score Display Seconds | 60 |
| Show Historical VI Lines | No |
| Show Compact Gauges | Yes |
| Enable Score Alerts | No |

## Why the first opening bar is excluded

A large opening auction or opening print can dominate the short rolling totals and make the order-flow readings misleading. Keeping **Exclude First Bar After Session Gap From Flow = Yes** reduces that problem.

The bar's price information can still contribute to price context, but its bid/ask flow is excluded.

---

# 6. Display settings

In the same **Settings and Inputs** list, find these entries:

```text
Text Horizontal Position (1-149)        3
Text Vertical Position Percent         97
Text Font Size                          8
Gauge Center Horizontal Position      128
Gauge Half Width                       17
```

Keep these values initially.

## What each display setting does

### Text Horizontal Position

```text
3
```

Controls the left-to-right position of the three-line text panel.

- Higher value moves the text right.
- Lower value moves the text left.

### Text Vertical Position Percent

```text
97
```

Controls the vertical position of the text panel.

- Higher value moves it toward the top.
- Lower value moves it downward.

If the text touches the chart's top data line, try:

```text
90
```

### Text Font Size

```text
8
```

Controls the text size.

Increase it only if the panel is difficult to read. A larger font uses more chart space.

### Gauge Center Horizontal Position

```text
128
```

Moves all four horizontal gauges together.

- Lower value moves them left.
- Higher value moves them right.

### Gauge Half Width

```text
17
```

Controls the width of the gauges.

If the gauges overlap the right-hand price scale, use:

```text
Gauge Center Horizontal Position      121
Gauge Half Width                       14
```

## Applying a display change

1. Select the input row.
2. Change its value in the input editor.
3. Click **Apply**.
4. Inspect the chart without closing the settings window.
5. Adjust again if needed.
6. Click **OK** when finished.

---

# 7. Region and scale settings

In the study's **Settings and Inputs** tab, keep:

```text
Based On:       Main Price Graph
Chart Region:   2
Value Format:   0.1
```

`Chart Region 2` is appropriate when it creates the separate bottom panel shown in your layout. On another chart, the region number may differ if other studies already occupy regions.

## Set the scale

1. In the Study Settings window, click **Scale**.
2. Use a linear, user-defined scale.
3. Set approximately:

```text
Top:       +100
Bottom:    -100
```

This keeps the gauges, histogram, zero line, and threshold lines visually consistent.

---

# 8. Optional cleanup of the study-name line

The custom text panel already identifies the signal, so the long study name shown across the top of the region may be unnecessary.

1. Open the study's **Settings**.
2. Open the **Subgraphs** tab.
3. Disable settings such as:

```text
Display Study Name
Display Input Values
Display Study Subgraphs Name and Value - Global
```

The exact wording can vary slightly by Sierra Chart version.

Do not disable the custom text panel or the compact gauges.

---

# 9. Readiness phases: START, EARLY, and LIVE

The indicator begins as quickly as possible without pretending that incomplete windows are fully valid.

## START

Example:

```text
START 12/15s
```

This means the short window has collected 12 seconds of the required 15 seconds before the first early score can be calculated.

The displayed score is not ready yet.

## EARLY

Example:

```text
EARLY 54%
```

The score is active, but the medium or long windows are not fully populated.

An asterisk after a window label means that window is partial:

```text
VI 20s -12 | 1m* -5 | 2m* +8
```

Treat EARLY readings as useful but lower-confidence information.

## LIVE

Example:

```text
LIVE
```

All three windows have sufficient coverage:

- 20 seconds
- 60 seconds
- 120 seconds

At this point, the score and state labels have full rolling-window context.

The indicator normally reaches LIVE approximately two minutes after the first usable trade data for the current session or replay segment.

---

# 10. The four horizontal gauges

There are four outlined horizontal boxes on the right side of the study panel.

From top to bottom, they are:

| Gauge | Meaning |
|---|---|
| Top | 20-second volume imbalance |
| Second | 1-minute volume imbalance |
| Third | 2-minute volume imbalance |
| Bottom | Composite absorption/reversal score |

Each gauge has a small vertical center mark representing zero.

```text
Negative / selling  ←  |  →  Positive / buying
                         0
```

## Gauge colors

- **Red extending left:** negative reading
- **Cyan extending right:** positive reading
- **Green extending right:** positive composite score with a recognized bullish absorption or bullish reversal state
- **Gray near the center:** neutral or nearly zero

## Gauge length

The gauge range is approximately -100 to +100.

A reading of `-9` only fills about 9% of the left half. A small-looking bar is therefore normal and does not mean the gauge is broken.

Example:

```text
VI 20s -9 | 1m -4 | 2m -5
```

The three top gauges should all appear slightly red and only a short distance left of center.

The bottom gauge may be longer because the composite score can be larger than any individual VI value.

---

# 11. Volume imbalance

The three top gauges and the `VI` text values use this formula:

```text
Volume Imbalance %
= 100 × (Ask Volume - Bid Volume)
        ÷ max(Ask Volume + Bid Volume, 1)
```

## Positive VI

Positive values mean more volume was classified as trading at the ask than at the bid.

This generally represents stronger buyer-initiated aggression.

## Negative VI

Negative values mean more volume was classified as trading at the bid than at the ask.

This generally represents stronger seller-initiated aggression.

## Near-zero VI

Values near zero mean bid-side and ask-side aggression are relatively balanced.

A near-zero reading does not automatically mean price is neutral. Price may still be moving because of liquidity conditions, spread changes, or the sequence of trades.

---

# 12. The historical histogram

The vertical bars across the lower panel show the **historical composite score**.

They are not raw volume bars.

On a 5-second chart, each histogram column normally represents approximately one 5-second chart bar.

## Histogram direction

- Bars above zero indicate bullish composite pressure.
- Bars below zero indicate bearish composite pressure.

## Histogram colors

- **Cyan above zero:** positive score, but no fully classified bullish absorption state yet
- **Green above zero:** bullish absorption or bullish reversal pressure was recognized
- **Red below zero:** negative composite pressure
- **Gray near zero:** neutral

## Reference lines

- Solid center line: `0`
- Upper dashed line: approximately `+35`
- Lower dashed line: approximately `-35`

The dashed lines correspond to the default absorption-state threshold.

## How to read a developing bullish turn

A useful progression can look like:

```text
Deep red bars
→ shorter red bars
→ bars rising toward zero
→ cyan bars above zero
→ green bars above +35
```

The transition is usually more informative than one isolated positive bar.

---

# 13. The live text panel

A typical reading looks like:

```text
Abs -34 | LIVE | Neutral
VI 20s -9 | 1m -4 | 2m -5 | Pace +2%
Px -0.14 / -2.00 / -1.03 | D -3.5K / -2.9K / -6.5K
```

## `Abs -34`

This is the composite absorption/reversal score.

Approximate scale:

```text
-100              0              +100
Bearish         Neutral          Bullish
```

- A positive score favors bullish absorption or reversal behavior.
- A negative score favors bearish behavior or the bearish mirror condition.
- A score near zero means the components are mixed or weak.

The score is not a prediction and should not be used alone as an automatic entry signal.

## `LIVE`

All three time windows are sufficiently populated.

Other possible phases are `START` and `EARLY`.

## State label

Possible labels include:

- Starting
- Missing bid/ask volume
- Low activity
- Neutral
- Strong sell pressure
- Sell pressure weakening
- Bullish absorption
- Bullish reversal pressure
- Strong buy pressure
- Buy pressure weakening
- Bearish absorption
- Bearish reversal pressure

The state label uses more conditions than the score alone. Therefore, a score can be near a threshold while the label still says `Neutral`.

For example:

```text
Abs -34 | LIVE | Neutral
```

This means the composite is moderately negative, but the complete requirements for a named bearish state were not met.

`Neutral` means “no high-quality classified condition,” not necessarily “nothing is happening.”

## `VI 20s -9 | 1m -4 | 2m -5`

These are the volume imbalance percentages for:

1. 20 seconds
2. 1 minute
3. 2 minutes

The short window should usually react first.

For a possible bullish turn after a sell flush, watch for the 20-second VI to become less negative or positive before the longer windows improve.

## `Pace +2%`

Pace compares the current 20-second trade rate with the previous 20-second trade rate.

- `+30%` means trades are arriving approximately 30% faster.
- `-30%` means trades are arriving approximately 30% slower.
- `--` means there is not enough prior-window data yet.

Pace has no direction by itself. Faster trading can be aggressively bullish or aggressively bearish.

Use price, VI, and delta to determine direction.

## `Px -0.14 / -2.00 / -1.03`

These are the price changes over:

1. 20 seconds
2. 1 minute
3. 2 minutes

The example means the current price is:

- $0.14 below its 20-second reference
- $2.00 below its 1-minute reference
- $1.03 below its 2-minute reference

The values do not need to increase smoothly because each window begins at a different time and price.

## `D -3.5K / -2.9K / -6.5K`

`D` means raw volume delta:

```text
Delta = Ask Volume - Bid Volume
```

The three values correspond to:

1. 20 seconds
2. 1 minute
3. 2 minutes

`K` means thousands of shares.

Example:

```text
-3.5K
```

means approximately 3,500 more shares were classified at the bid than at the ask during that window.

Delta gives the absolute share difference. VI converts that difference into a percentage of total classified volume.

---

# 14. How to interpret a possible bullish turn after a downward flush

Use the indicator as a sequence, not as a single number.

## Step 1: Confirm downside context

Look for one or more of the following:

- The 1-minute or 2-minute `Px` value is negative.
- Price recently made a meaningful low.
- The histogram has been below zero.
- Delta and VI show active selling.

## Step 2: Look for inefficient selling

The important clue is not necessarily positive delta.

Look for:

- Delta remains negative.
- VI remains negative.
- Price stops making meaningful new lows.
- The 20-second price change becomes less negative or turns positive.

This can mean sellers are still hitting bids but are no longer moving price efficiently.

## Step 3: Watch the short window improve first

The 20-second gauge should normally react before the 1-minute and 2-minute gauges.

A possible improvement sequence is:

```text
20s VI: -35 → -18 → -5 → +8
1m VI:  -28 → -24 → -17
2m VI:  -20 → -19 → -16
```

The longer windows may remain negative while the short window turns positive.

That is often more useful than waiting for all three windows to become positive.

## Step 4: Confirm price response

For a stronger bullish case:

- 20-second `Px` turns positive.
- Price reclaims a nearby level or prior micro high.
- Price holds above the flush low.
- Red histogram bars shorten and approach zero.

## Step 5: Check pace

A positive or elevated pace reading means participation remains active.

The most useful combination is:

```text
Active or accelerating pace
+ improving 20-second VI
+ price no longer falling
```

Falling pace can still produce a bounce, but it may indicate that the move is simply going quiet rather than being actively absorbed.

## Step 6: Watch the score and state

Default thresholds:

```text
Absorption threshold:      +35 / -35
Strong reversal threshold: +60 / -60
```

For a bullish setup:

- Above `+35`: stronger absorption evidence
- Above `+60`: stronger reversal pressure, if the price and flow conditions also agree

Useful bullish labels include:

- Sell pressure weakening
- Bullish absorption
- Bullish reversal pressure

Do not treat a threshold crossing as an automatic order. Confirm it with the actual price level, spread, liquidity, and your other indicators.

---

# 15. Bearish mirror interpretation

The study is symmetrical.

After an upward squeeze or fast rally, it can identify situations where:

- Aggressive buying remains high.
- Price stops advancing efficiently.
- Short-window price response turns downward.
- VI and delta weaken.
- The score moves negative.

Possible bearish labels include:

- Buy pressure weakening
- Bearish absorption
- Bearish reversal pressure

The four gauges work the same way: left/red is negative and right/cyan or green is positive.

---

# 16. Example interpretation

Example display:

```text
Abs -34 | LIVE | Neutral
VI 20s -9 | 1m -4 | 2m -5 | Pace +2%
Px -0.14 / -2.00 / -1.03 | D -3.5K / -2.9K / -6.5K
```

Interpretation:

- All three windows are active because the phase says `LIVE`.
- VI is slightly negative across all three windows.
- Sellers have a small aggression advantage, but not an extreme one.
- Price is negative across all three windows.
- Delta is negative across all three windows.
- Pace is only 2% faster than the previous short period.
- The composite score is moderately negative.
- The full requirements for a named bearish state were not met, so the label remains `Neutral`.

This is **not** a confirmed bullish reversal reading. It suggests continued caution and no strong bullish turn signal at that instant.

---

# 17. Replay setup for realistic testing

For quick visual review, standard replay is acceptable.

For closer testing of live fluctuations:

1. Open the Chart Replay control panel.
2. Find **Replay Mode**.
3. Select a mode named similar to:

```text
Calculate at Every Tick/Trade
```

4. Start at a moderate replay speed.
5. Watch how the 20-second VI, pace, price response, and score change around known flushes.

Trade-by-trade replay is more CPU-intensive than standard replay but is better for evaluating a fast scalping study.

---

# 18. Troubleshooting

## The study says START

Cause: fewer than 15 seconds of usable short-window data are available.

Action: allow replay or live data to continue.

## The study says EARLY

Cause: the score has started, but the 60-second or 120-second windows are not full.

Action: use the reading as lower-confidence information. Wait for `LIVE` when possible.

## The study remains START or shows missing bid/ask volume

Check:

1. The chart is intraday.
2. Intraday storage is set to `1 Tick`.
3. Bid Volume and Ask Volume are populated.
4. The correct market-data feed is connected.
5. Replay is moving rather than paused.

## Pace shows `--`

Cause: the previous 20-second comparison period is not sufficiently populated.

Action: allow more data to accumulate.

## The top three gauges look tiny

This is usually correct. They use a full -100 to +100 scale.

A VI value of `-5` should only create a small fill left of center.

## The gauges overlap the price scale

Change:

```text
Gauge Center Horizontal Position      121
Gauge Half Width                       14
```

## The text overlaps the study-name line

Change:

```text
Text Vertical Position Percent         90
```

Or hide the default study-name display from the **Subgraphs** tab.

## The panel is on the wrong chart

Check the symbol shown in the chart title. The study is calculated from the chart on which it is installed.

For example, a study on an `INTC` chart is analyzing INTC, even if nearby windows are showing SNOW.

## The old study appears instead of the fast version

The correct name is:

```text
Fast Scalp Flush Reversal / Absorption
```

Its default windows are:

```text
20 / 60 / 120 seconds
```

Remove and re-add the study after rebuilding the DLL if the input list does not match.

---

# 19. Practical quick-reference checklist

After a downward flush, look for:

1. The 1-minute or 2-minute price change remains negative.
2. The 20-second price change stops falling or turns positive.
3. The top 20-second VI gauge improves first.
4. Pace remains active or accelerates.
5. Delta may remain negative, but price stops responding downward.
6. Red histogram bars shorten toward zero.
7. The score moves above `+35`.
8. The label changes to `Sell pressure weakening`, `Bullish absorption`, or `Bullish reversal pressure`.
9. Price confirms by holding or reclaiming a meaningful level.

Avoid acting only because one bar turns cyan or because the score briefly crosses a threshold.

---

# 20. Important limitation

This indicator summarizes recent trades and price response. It cannot know whether a level will hold, whether hidden liquidity will remain, or whether new information will suddenly change the market.

Use it as a fast decision-support tool alongside:

- Actual price action
- Spread and liquidity
- Bookmap heatmap and volume dots
- Cumulative volume delta
- Your existing Volume Delta, Pace, and Price Change studies
- Defined risk and stop placement

The most important signal is often the relationship between aggressive volume and price response, not the sign of one number by itself.
