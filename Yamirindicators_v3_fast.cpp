#include "sierrachart.h"

SCDLLName("Yamir Indicators")

namespace
{
    const double YI_SECONDS_PER_DAY = 86400.0;

    double YI_Clamp(const double Value, const double Minimum, const double Maximum)
    {
        if (Value < Minimum)
            return Minimum;
        if (Value > Maximum)
            return Maximum;
        return Value;
    }

    double YI_Max(const double A, const double B)
    {
        return A > B ? A : B;
    }

    double YI_Min(const double A, const double B)
    {
        return A < B ? A : B;
    }

    double YI_Abs(const double Value)
    {
        return Value >= 0.0 ? Value : -Value;
    }

    int YI_RoundToInt(const double Value)
    {
        return static_cast<int>(Value >= 0.0 ? Value + 0.5 : Value - 0.5);
    }

    double YI_DateTimeDifferenceInSeconds(const SCDateTime& Newer, const SCDateTime& Older)
    {
        return (Newer.GetAsDouble() - Older.GetAsDouble()) * YI_SECONDS_PER_DAY;
    }

    struct YI_WindowMetrics
    {
        double AskVolume;
        double BidVolume;
        double TotalVolume;
        double ChartVolume;
        double ClassifiedVolumePct;
        double Delta;
        double ImbalancePct;

        double PreviousAskVolume;
        double PreviousBidVolume;
        double PreviousTotalVolume;
        double PreviousChartVolume;
        double PreviousDelta;
        double PreviousImbalancePct;

        double Trades;
        double PreviousTrades;
        double PaceAccelerationPct;

        double PriceChange;
        double PriceTicks;
        double ReferencePrice;

        double Low;
        double High;
        double ReboundFromLowTicks;
        double PullbackFromHighTicks;
        double LowAgeSeconds;
        double HighAgeSeconds;

        double CurrentCoverageSeconds;
        double PreviousCoverageSeconds;

        int StartIndex;
        int PreviousStartIndex;
        int LowIndex;
        int HighIndex;

        int HasCurrentHistory;
        int HasPreviousHistory;
        int HasBidAskData;
    };

    struct YI_BaselineMetrics
    {
        double ActiveSeconds;
        double CoverageSeconds;
        double SumSquaredTicks;
        double TotalTrades;
        double TotalBidAskVolume;
        double SigmaTicksPerSqrtSecond;
        double AverageTradesPerSecond;
        double AverageVolumePerSecond;
        int Ready;
    };

    SCDateTime YI_GetWindowEndDateTime(
        SCStudyInterfaceRef sc,
        const int Index)
    {
        SCDateTime Result = sc.BaseDateTimeIn[Index];

        if (Index == sc.ArraySize - 1
            && sc.LatestDateTimeForLastBar.GetAsDouble() > Result.GetAsDouble())
        {
            Result = sc.LatestDateTimeForLastBar;
        }
        else if (Index + 1 < sc.ArraySize)
        {
            const double GapSeconds = YI_DateTimeDifferenceInSeconds(
                sc.BaseDateTimeIn[Index + 1],
                sc.BaseDateTimeIn[Index]);

            if (GapSeconds > 0.0 && GapSeconds <= 60.0)
                Result = sc.BaseDateTimeIn[Index + 1];
        }

        return Result;
    }

    YI_WindowMetrics YI_CalculateWindowMetrics(
        SCStudyInterfaceRef sc,
        const int Index,
        const int WindowSeconds,
        const double TickSize)
    {
        YI_WindowMetrics Result = {};

        Result.StartIndex = Index;
        Result.PreviousStartIndex = -1;
        Result.LowIndex = Index;
        Result.HighIndex = Index;
        Result.Low = sc.Low[Index];
        Result.High = sc.High[Index];

        const SCDateTime CurrentDateTime = YI_GetWindowEndDateTime(sc, Index);

        SCDateTime CurrentWindowThreshold = CurrentDateTime;
        CurrentWindowThreshold -= SCDateTime::SECONDS(WindowSeconds);

        SCDateTime PreviousWindowThreshold = CurrentDateTime;
        PreviousWindowThreshold -= SCDateTime::SECONDS(2 * WindowSeconds);

        int PreviousNewestIndex = -1;

        for (int BarIndex = Index; BarIndex >= 0; --BarIndex)
        {
            const SCDateTime BarDateTime = sc.BaseDateTimeIn[BarIndex];

            if (!(BarDateTime >= PreviousWindowThreshold))
                break;

            const double AskVolume = sc.BaseData[SC_ASKVOL][BarIndex];
            const double BidVolume = sc.BaseData[SC_BIDVOL][BarIndex];
            const double Trades = sc.NumberOfTrades[BarIndex];

            if (BarDateTime >= CurrentWindowThreshold)
            {
                Result.AskVolume += AskVolume;
                Result.BidVolume += BidVolume;
                Result.ChartVolume += sc.Volume[BarIndex];
                Result.Trades += Trades;
                Result.StartIndex = BarIndex;

                if (sc.Low[BarIndex] < Result.Low)
                {
                    Result.Low = sc.Low[BarIndex];
                    Result.LowIndex = BarIndex;
                }

                if (sc.High[BarIndex] > Result.High)
                {
                    Result.High = sc.High[BarIndex];
                    Result.HighIndex = BarIndex;
                }
            }
            else
            {
                Result.PreviousAskVolume += AskVolume;
                Result.PreviousBidVolume += BidVolume;
                Result.PreviousChartVolume += sc.Volume[BarIndex];
                Result.PreviousTrades += Trades;
                Result.PreviousStartIndex = BarIndex;

                if (PreviousNewestIndex < 0)
                    PreviousNewestIndex = BarIndex;
            }
        }

        Result.TotalVolume = Result.AskVolume + Result.BidVolume;
        Result.ClassifiedVolumePct = 100.0 * Result.TotalVolume
            / YI_Max(Result.ChartVolume, 1.0);
        Result.Delta = Result.AskVolume - Result.BidVolume;
        Result.ImbalancePct = 100.0 * Result.Delta / YI_Max(Result.TotalVolume, 1.0);

        Result.PreviousTotalVolume = Result.PreviousAskVolume + Result.PreviousBidVolume;
        Result.PreviousDelta = Result.PreviousAskVolume - Result.PreviousBidVolume;
        Result.PreviousImbalancePct = 100.0 * Result.PreviousDelta / YI_Max(Result.PreviousTotalVolume, 1.0);

        // Use the opening price of the oldest included bar. This avoids
        // accidentally using the prior session close during the first few
        // minutes after the equity market opens.
        Result.ReferencePrice = sc.Open[Result.StartIndex];
        Result.PriceChange = sc.Close[Index] - Result.ReferencePrice;
        Result.PriceTicks = Result.PriceChange / TickSize;

        Result.ReboundFromLowTicks = (sc.Close[Index] - Result.Low) / TickSize;
        Result.PullbackFromHighTicks = (Result.High - sc.Close[Index]) / TickSize;
        Result.LowAgeSeconds = YI_DateTimeDifferenceInSeconds(CurrentDateTime, sc.BaseDateTimeIn[Result.LowIndex]);
        Result.HighAgeSeconds = YI_DateTimeDifferenceInSeconds(CurrentDateTime, sc.BaseDateTimeIn[Result.HighIndex]);

        Result.CurrentCoverageSeconds = YI_DateTimeDifferenceInSeconds(CurrentDateTime, sc.BaseDateTimeIn[Result.StartIndex]);
        Result.HasCurrentHistory = Result.CurrentCoverageSeconds >= 0.85 * WindowSeconds;

        if (Result.PreviousStartIndex >= 0 && PreviousNewestIndex >= 0)
        {
            Result.PreviousCoverageSeconds = YI_DateTimeDifferenceInSeconds(
                CurrentWindowThreshold,
                sc.BaseDateTimeIn[Result.PreviousStartIndex]);
            Result.HasPreviousHistory = Result.PreviousCoverageSeconds >= 0.85 * WindowSeconds;
        }

        if (Result.HasPreviousHistory)
        {
            Result.PaceAccelerationPct = 100.0
                * (Result.Trades - Result.PreviousTrades)
                / YI_Max(Result.PreviousTrades, 1.0);
        }

        Result.HasBidAskData = Result.TotalVolume > 0.0;

        return Result;
    }

    YI_BaselineMetrics YI_CalculateBaselineMetrics(
        SCStudyInterfaceRef sc,
        const int Index,
        const int BaselineSeconds,
        const double TickSize)
    {
        YI_BaselineMetrics Result = {};

        if (Index < 1)
            return Result;

        const SCDateTime CurrentDateTime = YI_GetWindowEndDateTime(sc, Index);
        SCDateTime Threshold = CurrentDateTime;
        Threshold -= SCDateTime::SECONDS(BaselineSeconds);

        int OldestIncludedIndex = Index;

        for (int BarIndex = Index; BarIndex > 0; --BarIndex)
        {
            if (sc.BaseDateTimeIn[BarIndex - 1] <= Threshold)
                break;

            const double ElapsedSeconds = YI_DateTimeDifferenceInSeconds(
                sc.BaseDateTimeIn[BarIndex],
                sc.BaseDateTimeIn[BarIndex - 1]);

            // Ignore very large time gaps, such as overnight gaps, in the
            // realized-variance and pace baselines.
            if (ElapsedSeconds > 0.0 && ElapsedSeconds <= 300.0)
            {
                const double ChangeTicks = (sc.Close[BarIndex] - sc.Close[BarIndex - 1]) / TickSize;
                Result.SumSquaredTicks += ChangeTicks * ChangeTicks;
                Result.ActiveSeconds += ElapsedSeconds;
                Result.TotalTrades += sc.NumberOfTrades[BarIndex];
                Result.TotalBidAskVolume += sc.BaseData[SC_ASKVOL][BarIndex]
                    + sc.BaseData[SC_BIDVOL][BarIndex];
            }

            OldestIncludedIndex = BarIndex - 1;
        }

        Result.CoverageSeconds = YI_DateTimeDifferenceInSeconds(
            CurrentDateTime,
            sc.BaseDateTimeIn[OldestIncludedIndex]);

        if (Result.ActiveSeconds > 0.0)
        {
            Result.SigmaTicksPerSqrtSecond = sqrt(
                Result.SumSquaredTicks / Result.ActiveSeconds);
            Result.AverageTradesPerSecond = Result.TotalTrades / Result.ActiveSeconds;
            Result.AverageVolumePerSecond = Result.TotalBidAskVolume / Result.ActiveSeconds;
        }

        const double MinimumRequiredCoverage = YI_Min(300.0, 0.25 * BaselineSeconds);
        Result.Ready = Result.CoverageSeconds >= MinimumRequiredCoverage
            && Result.ActiveSeconds >= 60.0;

        return Result;
    }

    double YI_ExpectedMoveTicks(
        const int WindowSeconds,
        const int LongWindowSeconds,
        const double MinimumLongFlushTicks,
        const int UseAdaptiveThreshold,
        const double AdaptiveSigmaMultiplier,
        const YI_BaselineMetrics& Baseline)
    {
        const double TimeRatio = static_cast<double>(WindowSeconds)
            / YI_Max(static_cast<double>(LongWindowSeconds), 1.0);

        const double ManualFloor = YI_Max(
            1.0,
            MinimumLongFlushTicks * sqrt(YI_Max(TimeRatio, 0.0001)));

        if (!UseAdaptiveThreshold || !Baseline.Ready)
            return ManualFloor;

        const double AdaptiveMove = AdaptiveSigmaMultiplier
            * Baseline.SigmaTicksPerSqrtSecond
            * sqrt(static_cast<double>(WindowSeconds));

        return YI_Max(ManualFloor, AdaptiveMove);
    }

    void YI_FormatWindowLabel(const int Seconds, SCString& Output)
    {
        if (Seconds % 60 == 0)
            Output.Format("%dm", Seconds / 60);
        else
            Output.Format("%ds", Seconds);
    }

    void YI_FormatSignedCompactNumber(const double Value, SCString& Output)
    {
        const double AbsoluteValue = YI_Abs(Value);

        if (AbsoluteValue >= 1000000.0)
            Output.Format("%+.1fM", Value / 1000000.0);
        else if (AbsoluteValue >= 1000.0)
            Output.Format("%+.1fK", Value / 1000.0);
        else
            Output.Format("%+.0f", Value);
    }

    void YI_DrawRelativeRectangle(
        SCStudyInterfaceRef sc,
        const int LineNumber,
        int X1,
        int X2,
        const float Y1,
        const float Y2,
        const unsigned int Color,
        const int Transparency)
    {
        X1 = static_cast<int>(YI_Clamp(X1, 1, 149));
        X2 = static_cast<int>(YI_Clamp(X2, 1, 149));

        if (X1 == X2)
        {
            if (X2 < 149)
                ++X2;
            else
                --X1;
        }

        if (X1 > X2)
        {
            const int Temporary = X1;
            X1 = X2;
            X2 = Temporary;
        }

        s_UseTool Tool;
        Tool.Clear();
        Tool.ChartNumber = sc.ChartNumber;
        Tool.DrawingType = DRAWING_RECTANGLEHIGHLIGHT;
        Tool.AddMethod = UTAM_ADD_OR_ADJUST;
        Tool.LineNumber = LineNumber;
        Tool.BeginDateTime = X1;
        Tool.EndDateTime = X2;
        Tool.UseRelativeVerticalValues = 1;
        Tool.BeginValue = Y1;
        Tool.EndValue = Y2;
        Tool.Region = sc.GraphRegion;
        Tool.Color = Color;
        Tool.SecondaryColor = Color;
        Tool.LineWidth = 1;
        Tool.TransparencyLevel = Transparency;
        Tool.DrawOutlineOnly = 0;
        sc.UseTool(Tool);
    }

    void YI_DrawGauge(
        SCStudyInterfaceRef sc,
        const int BaseLineNumber,
        const int CenterX,
        const int HalfWidth,
        const float BottomY,
        const float TopY,
        const double Value,
        const unsigned int PositiveColor,
        const unsigned int NegativeColor,
        const unsigned int NeutralColor)
    {
        const int LeftX = CenterX - HalfWidth;
        const int RightX = CenterX + HalfWidth;

        YI_DrawRelativeRectangle(
            sc,
            BaseLineNumber,
            LeftX,
            RightX,
            BottomY,
            TopY,
            RGB(70, 78, 84),
            82);

        YI_DrawRelativeRectangle(
            sc,
            BaseLineNumber + 1,
            CenterX - 1,
            CenterX + 1,
            BottomY,
            TopY,
            RGB(145, 150, 155),
            35);

        const double ClampedValue = YI_Clamp(Value, -100.0, 100.0);
        int EndX = CenterX + YI_RoundToInt(HalfWidth * ClampedValue / 100.0);

        unsigned int FillColor = NeutralColor;
        if (ClampedValue > 0.5)
            FillColor = PositiveColor;
        else if (ClampedValue < -0.5)
            FillColor = NegativeColor;

        YI_DrawRelativeRectangle(
            sc,
            BaseLineNumber + 2,
            CenterX,
            EndX,
            BottomY + 0.7f,
            TopY - 0.7f,
            FillColor,
            18);
    }

    void YI_DrawTextPanel(
        SCStudyInterfaceRef sc,
        const int LineNumber,
        const int HorizontalPosition,
        const float VerticalPosition,
        const int FontSize,
        const unsigned int Color,
        const SCString& Text)
    {
        s_UseTool Tool;
        Tool.Clear();
        Tool.ChartNumber = sc.ChartNumber;
        Tool.DrawingType = DRAWING_TEXT;
        Tool.AddMethod = UTAM_ADD_OR_ADJUST;
        Tool.LineNumber = LineNumber;
        Tool.BeginDateTime = static_cast<int>(YI_Clamp(HorizontalPosition, 1, 149));
        Tool.UseRelativeVerticalValues = 1;
        Tool.BeginValue = static_cast<float>(YI_Clamp(VerticalPosition, 1.0, 100.0));
        Tool.Region = sc.GraphRegion;
        Tool.Color = Color;
        Tool.FontSize = FontSize;
        Tool.FontBold = 1;
        Tool.TextAlignment = DT_LEFT | DT_TOP;
        Tool.MultiLineLabel = 1;
        Tool.TransparentLabelBackground = 1;
        Tool.Text = Text;
        sc.UseTool(Tool);
    }
}

// Existing study: preserved.
SCSFExport scsf_VolumeDeltaIndicator(SCStudyInterfaceRef sc)
{
    if (sc.SetDefaults)
    {
        sc.GraphName = "Volume Delta Indicator";
        sc.AutoLoop = 1;
        sc.GraphRegion = 0;

        SCSubgraphRef SubgraphDelta = sc.Subgraph[0];
        SubgraphDelta.Name = "Volume Delta";
        SubgraphDelta.DrawStyle = DRAWSTYLE_BAR;
        SubgraphDelta.PrimaryColor = RGB(0, 191, 255);
        SubgraphDelta.SecondaryColor = RGB(255, 0, 0);
        SubgraphDelta.SecondaryColorUsed = 1;
        SubgraphDelta.LineWidth = 2;

        SCInputRef InputDisplayAsBars = sc.Input[0];
        InputDisplayAsBars.Name = "Display as Bars";
        InputDisplayAsBars.SetYesNo(1);

        return;
    }

    SCSubgraphRef SubgraphDelta = sc.Subgraph[0];
    SCInputRef InputDisplayAsBars = sc.Input[0];

    const int Index = sc.Index;
    SCFloatArrayRef BidVolume = sc.BaseData[SC_BIDVOL];
    SCFloatArrayRef AskVolume = sc.BaseData[SC_ASKVOL];

    const float VolumeDelta = AskVolume[Index] - BidVolume[Index];
    SubgraphDelta[Index] = VolumeDelta;
    SubgraphDelta.DataColor[Index] = VolumeDelta >= 0.0f
        ? SubgraphDelta.PrimaryColor
        : SubgraphDelta.SecondaryColor;

    SubgraphDelta.DrawStyle = InputDisplayAsBars.GetYesNo()
        ? DRAWSTYLE_BAR
        : DRAWSTYLE_LINE;
}

// Existing study: preserved, with the early-index safety defect corrected.
SCSFExport scsf_CustomTapeReader(SCStudyInterfaceRef sc)
{
    SCInputRef Input_LookbackPeriod = sc.Input[0];
    SCSubgraphRef Subgraph_TapeReader = sc.Subgraph[0];

    if (sc.SetDefaults)
    {
        sc.GraphName = "Custom Pace of Tape Reader";
        sc.AutoLoop = 1;
        sc.FreeDLL = 0;
        sc.UpdateAlways = 1;

        Subgraph_TapeReader.Name = "Tape Reader";
        Subgraph_TapeReader.DrawStyle = DRAWSTYLE_BAR;
        Subgraph_TapeReader.PrimaryColor = RGB(0, 192, 255);
        Subgraph_TapeReader.SecondaryColorUsed = 1;
        Subgraph_TapeReader.SecondaryColor = RGB(255, 0, 0);
        Subgraph_TapeReader.LineWidth = 2;

        Input_LookbackPeriod.Name = "Lookback Period";
        Input_LookbackPeriod.SetInt(12);
        Input_LookbackPeriod.SetIntLimits(1, 100);

        return;
    }

    const int LookbackPeriod = Input_LookbackPeriod.GetInt();
    int TradeCountChange = 0;

    // Both the current and prior blocks require 2 * LookbackPeriod bars.
    if (sc.Index >= 2 * LookbackPeriod - 1)
    {
        int TradeCountPreviousPeriod = 0;
        int TradeCountCurrentPeriod = 0;

        for (int Offset = 0; Offset < LookbackPeriod; ++Offset)
            TradeCountCurrentPeriod += static_cast<int>(sc.NumberOfTrades[sc.Index - Offset]);

        for (int Offset = LookbackPeriod; Offset < 2 * LookbackPeriod; ++Offset)
            TradeCountPreviousPeriod += static_cast<int>(sc.NumberOfTrades[sc.Index - Offset]);

        TradeCountChange = TradeCountCurrentPeriod - TradeCountPreviousPeriod;
    }

    Subgraph_TapeReader[sc.Index] = static_cast<float>(TradeCountChange);
    Subgraph_TapeReader.DataColor[sc.Index] = TradeCountChange >= 0
        ? Subgraph_TapeReader.PrimaryColor
        : Subgraph_TapeReader.SecondaryColor;
}

// Existing study: preserved.
SCSFExport scsf_CustomPriceChangeTapeReader(SCStudyInterfaceRef sc)
{
    SCInputRef Input_LookbackPeriod = sc.Input[0];
    SCSubgraphRef Subgraph_TapeReader = sc.Subgraph[0];

    if (sc.SetDefaults)
    {
        sc.GraphName = "Custom Price Change Tape Reader";
        sc.AutoLoop = 1;
        sc.FreeDLL = 0;
        sc.UpdateAlways = 1;

        Subgraph_TapeReader.Name = "Tape Reader";
        Subgraph_TapeReader.DrawStyle = DRAWSTYLE_BAR;
        Subgraph_TapeReader.PrimaryColor = RGB(0, 192, 255);
        Subgraph_TapeReader.SecondaryColorUsed = 1;
        Subgraph_TapeReader.SecondaryColor = RGB(255, 0, 0);
        Subgraph_TapeReader.LineWidth = 4;

        Input_LookbackPeriod.Name = "Lookback Period";
        Input_LookbackPeriod.SetInt(12);
        Input_LookbackPeriod.SetIntLimits(1, 100);

        return;
    }

    const int LookbackPeriod = Input_LookbackPeriod.GetInt();
    float PriceChange = 0.0f;

    if (sc.Index >= LookbackPeriod)
        PriceChange = sc.Close[sc.Index] - sc.Close[sc.Index - LookbackPeriod];

    Subgraph_TapeReader[sc.Index] = PriceChange;
    Subgraph_TapeReader.DataColor[sc.Index] = PriceChange >= 0.0f
        ? Subgraph_TapeReader.PrimaryColor
        : Subgraph_TapeReader.SecondaryColor;
}

SCSFExport scsf_MultiHorizonFlushReversalAbsorption(SCStudyInterfaceRef sc)
{
    SCSubgraphRef Score = sc.Subgraph[0];
    SCSubgraphRef VIShort = sc.Subgraph[1];
    SCSubgraphRef VIMedium = sc.Subgraph[2];
    SCSubgraphRef VILong = sc.Subgraph[3];
    SCSubgraphRef ZeroLine = sc.Subgraph[4];
    SCSubgraphRef PositiveThreshold = sc.Subgraph[5];
    SCSubgraphRef NegativeThreshold = sc.Subgraph[6];
    SCSubgraphRef PaceAcceleration = sc.Subgraph[7];
    SCSubgraphRef PriceShort = sc.Subgraph[8];
    SCSubgraphRef PriceMedium = sc.Subgraph[9];
    SCSubgraphRef PriceLong = sc.Subgraph[10];
    SCSubgraphRef DeltaShort = sc.Subgraph[11];
    SCSubgraphRef DeltaMedium = sc.Subgraph[12];
    SCSubgraphRef DeltaLong = sc.Subgraph[13];
    SCSubgraphRef TradesShort = sc.Subgraph[14];
    SCSubgraphRef StateCode = sc.Subgraph[15];
    SCSubgraphRef AbsorptionComponent = sc.Subgraph[16];
    SCSubgraphRef DownFlushContext = sc.Subgraph[17];
    SCSubgraphRef UpFlushContext = sc.Subgraph[18];
    SCSubgraphRef DeltaIntensity = sc.Subgraph[19];

    SCInputRef InputShortSeconds = sc.Input[0];
    SCInputRef InputMediumSeconds = sc.Input[1];
    SCInputRef InputLongSeconds = sc.Input[2];
    SCInputRef InputBaselineSeconds = sc.Input[3];
    SCInputRef InputMinimumVolume = sc.Input[4];
    SCInputRef InputMinimumTrades = sc.Input[5];
    SCInputRef InputDisplayMode = sc.Input[6];
    SCInputRef InputShowHistoricalVI = sc.Input[7];
    SCInputRef InputVINormalizationPct = sc.Input[8];
    SCInputRef InputUseAdaptiveThreshold = sc.Input[9];
    SCInputRef InputAdaptiveSigmaMultiplier = sc.Input[10];
    SCInputRef InputMinimumFlushTicks = sc.Input[11];
    SCInputRef InputFlushRecencySeconds = sc.Input[12];
    SCInputRef InputAbsorptionThreshold = sc.Input[13];
    SCInputRef InputStrongThreshold = sc.Input[14];
    SCInputRef InputEnableAlerts = sc.Input[15];
    SCInputRef InputAlertThreshold = sc.Input[16];
    SCInputRef InputAlertNumber = sc.Input[17];
    SCInputRef InputAlertCooldownSeconds = sc.Input[18];
    SCInputRef InputShowGauges = sc.Input[19];
    SCInputRef InputTextHorizontalPosition = sc.Input[20];
    SCInputRef InputTextVerticalPosition = sc.Input[21];
    SCInputRef InputTextFontSize = sc.Input[22];
    SCInputRef InputGaugeCenterX = sc.Input[23];
    SCInputRef InputGaugeHalfWidth = sc.Input[24];

    if (sc.SetDefaults)
    {
        sc.GraphName = "Multi-Horizon Flush Reversal / Absorption";
        sc.StudyDescription = "Time-based 1m/3m/5m bid-ask volume imbalance with a contextual flush-reversal and absorption score.";
        sc.AutoLoop = 1;
        sc.GraphRegion = 1;
        sc.UpdateAlways = 1;
        sc.FreeDLL = 0;
        sc.ScaleRangeType = SCALE_USERDEFINED;
        sc.ScaleRangeTop = 100.0f;
        sc.ScaleRangeBottom = -100.0f;
        sc.ValueFormat = 1;

        Score.Name = "Absorption / Reversal Score";
        Score.DrawStyle = DRAWSTYLE_BAR;
        Score.PrimaryColor = RGB(0, 210, 120);
        Score.SecondaryColor = RGB(255, 55, 55);
        Score.SecondaryColorUsed = 1;
        Score.LineWidth = 3;
        Score.DrawZeros = 0;

        VIShort.Name = "Volume Imbalance Short";
        VIShort.DrawStyle = DRAWSTYLE_IGNORE;
        VIShort.PrimaryColor = RGB(0, 200, 255);
        VIShort.SecondaryColor = RGB(255, 70, 70);
        VIShort.SecondaryColorUsed = 1;
        VIShort.LineWidth = 2;
        VIShort.DrawZeros = 0;

        VIMedium.Name = "Volume Imbalance Medium";
        VIMedium.DrawStyle = DRAWSTYLE_IGNORE;
        VIMedium.PrimaryColor = RGB(70, 185, 255);
        VIMedium.SecondaryColor = RGB(235, 90, 90);
        VIMedium.SecondaryColorUsed = 1;
        VIMedium.LineWidth = 1;
        VIMedium.DrawZeros = 0;

        VILong.Name = "Volume Imbalance Long";
        VILong.DrawStyle = DRAWSTYLE_IGNORE;
        VILong.PrimaryColor = RGB(100, 165, 235);
        VILong.SecondaryColor = RGB(210, 110, 110);
        VILong.SecondaryColorUsed = 1;
        VILong.LineWidth = 1;
        VILong.DrawZeros = 0;

        ZeroLine.Name = "Zero";
        ZeroLine.DrawStyle = DRAWSTYLE_LINE;
        ZeroLine.PrimaryColor = RGB(125, 130, 135);
        ZeroLine.LineWidth = 1;
        ZeroLine.LineStyle = LINESTYLE_SOLID;
        ZeroLine.DrawZeros = 1;

        PositiveThreshold.Name = "Bullish Score Threshold";
        PositiveThreshold.DrawStyle = DRAWSTYLE_LINE;
        PositiveThreshold.PrimaryColor = RGB(0, 125, 85);
        PositiveThreshold.LineWidth = 1;
        PositiveThreshold.LineStyle = LINESTYLE_DASH;

        NegativeThreshold.Name = "Bearish Score Threshold";
        NegativeThreshold.DrawStyle = DRAWSTYLE_LINE;
        NegativeThreshold.PrimaryColor = RGB(145, 45, 45);
        NegativeThreshold.LineWidth = 1;
        NegativeThreshold.LineStyle = LINESTYLE_DASH;

        PaceAcceleration.Name = "Pace Acceleration Percent";
        PaceAcceleration.DrawStyle = DRAWSTYLE_IGNORE;

        PriceShort.Name = "Price Change Short";
        PriceShort.DrawStyle = DRAWSTYLE_IGNORE;
        PriceMedium.Name = "Price Change Medium";
        PriceMedium.DrawStyle = DRAWSTYLE_IGNORE;
        PriceLong.Name = "Price Change Long";
        PriceLong.DrawStyle = DRAWSTYLE_IGNORE;

        DeltaShort.Name = "Volume Delta Short";
        DeltaShort.DrawStyle = DRAWSTYLE_IGNORE;
        DeltaMedium.Name = "Volume Delta Medium";
        DeltaMedium.DrawStyle = DRAWSTYLE_IGNORE;
        DeltaLong.Name = "Volume Delta Long";
        DeltaLong.DrawStyle = DRAWSTYLE_IGNORE;

        TradesShort.Name = "Trade Count Short";
        TradesShort.DrawStyle = DRAWSTYLE_IGNORE;
        StateCode.Name = "State Code";
        StateCode.DrawStyle = DRAWSTYLE_IGNORE;
        AbsorptionComponent.Name = "Absorption Component";
        AbsorptionComponent.DrawStyle = DRAWSTYLE_IGNORE;
        DownFlushContext.Name = "Down Flush Context";
        DownFlushContext.DrawStyle = DRAWSTYLE_IGNORE;
        UpFlushContext.Name = "Up Flush Context";
        UpFlushContext.DrawStyle = DRAWSTYLE_IGNORE;
        DeltaIntensity.Name = "Delta Magnitude Intensity";
        DeltaIntensity.DrawStyle = DRAWSTYLE_IGNORE;

        InputShortSeconds.Name = "Short Window Seconds";
        InputShortSeconds.SetInt(60);
        InputShortSeconds.SetIntLimits(5, 3600);

        InputMediumSeconds.Name = "Medium Window Seconds";
        InputMediumSeconds.SetInt(180);
        InputMediumSeconds.SetIntLimits(10, 7200);

        InputLongSeconds.Name = "Long Window Seconds";
        InputLongSeconds.SetInt(300);
        InputLongSeconds.SetIntLimits(15, 14400);

        InputBaselineSeconds.Name = "Adaptive Baseline Seconds";
        InputBaselineSeconds.SetInt(1800);
        InputBaselineSeconds.SetIntLimits(300, 14400);

        InputMinimumVolume.Name = "Minimum Bid+Ask Volume In Short Window";
        InputMinimumVolume.SetInt(5000);
        InputMinimumVolume.SetIntLimits(0, 2000000000);

        InputMinimumTrades.Name = "Minimum Trade Count In Short Window";
        InputMinimumTrades.SetInt(50);
        InputMinimumTrades.SetIntLimits(0, 10000000);

        InputDisplayMode.Name = "Display Mode";
        InputDisplayMode.SetCustomInputStrings("Both;Raw Imbalance Only;Composite Score Only");
        InputDisplayMode.SetCustomInputIndex(0);

        InputShowHistoricalVI.Name = "Show Historical VI Lines";
        InputShowHistoricalVI.SetYesNo(0);

        InputVINormalizationPct.Name = "Meaningful VI Percent For Score Normalization";
        InputVINormalizationPct.SetFloat(20.0f);
        InputVINormalizationPct.SetFloatLimits(1.0f, 100.0f);

        InputUseAdaptiveThreshold.Name = "Use Adaptive Volatility Threshold";
        InputUseAdaptiveThreshold.SetYesNo(1);

        InputAdaptiveSigmaMultiplier.Name = "Adaptive Flush Sigma Multiplier";
        InputAdaptiveSigmaMultiplier.SetFloat(1.25f);
        InputAdaptiveSigmaMultiplier.SetFloatLimits(0.10f, 10.0f);

        InputMinimumFlushTicks.Name = "Minimum Long-Window Flush Size In Ticks";
        InputMinimumFlushTicks.SetFloat(8.0f);
        InputMinimumFlushTicks.SetFloatLimits(1.0f, 1000.0f);

        InputFlushRecencySeconds.Name = "Flush High/Low Recency Seconds";
        InputFlushRecencySeconds.SetInt(90);
        InputFlushRecencySeconds.SetIntLimits(10, 3600);

        InputAbsorptionThreshold.Name = "Absorption State Score Threshold";
        InputAbsorptionThreshold.SetFloat(45.0f);
        InputAbsorptionThreshold.SetFloatLimits(1.0f, 99.0f);

        InputStrongThreshold.Name = "Reversal State Score Threshold";
        InputStrongThreshold.SetFloat(70.0f);
        InputStrongThreshold.SetFloatLimits(1.0f, 100.0f);

        InputEnableAlerts.Name = "Enable Score Threshold Alerts";
        InputEnableAlerts.SetYesNo(0);

        InputAlertThreshold.Name = "Alert Absolute Score Threshold";
        InputAlertThreshold.SetFloat(70.0f);
        InputAlertThreshold.SetFloatLimits(1.0f, 100.0f);

        InputAlertNumber.Name = "Alert Sound Number";
        InputAlertNumber.SetInt(1);
        InputAlertNumber.SetIntLimits(1, 150);

        InputAlertCooldownSeconds.Name = "Alert Cooldown Seconds";
        InputAlertCooldownSeconds.SetInt(30);
        InputAlertCooldownSeconds.SetIntLimits(0, 3600);

        InputShowGauges.Name = "Show Compact Current-Value Gauges";
        InputShowGauges.SetYesNo(1);

        InputTextHorizontalPosition.Name = "Text Horizontal Position (1-149)";
        InputTextHorizontalPosition.SetInt(4);
        InputTextHorizontalPosition.SetIntLimits(1, 149);

        InputTextVerticalPosition.Name = "Text Vertical Position Percent";
        InputTextVerticalPosition.SetFloat(98.0f);
        InputTextVerticalPosition.SetFloatLimits(1.0f, 100.0f);

        InputTextFontSize.Name = "Text Font Size";
        InputTextFontSize.SetInt(9);
        InputTextFontSize.SetIntLimits(6, 24);

        InputGaugeCenterX.Name = "Gauge Center Horizontal Position (1-149)";
        InputGaugeCenterX.SetInt(126);
        InputGaugeCenterX.SetIntLimits(10, 140);

        InputGaugeHalfWidth.Name = "Gauge Half Width";
        InputGaugeHalfWidth.SetInt(20);
        InputGaugeHalfWidth.SetIntLimits(4, 50);

        return;
    }

    int ShortSeconds = InputShortSeconds.GetInt();
    int MediumSeconds = InputMediumSeconds.GetInt();
    int LongSeconds = InputLongSeconds.GetInt();

    if (MediumSeconds <= ShortSeconds)
        MediumSeconds = ShortSeconds + 1;
    if (LongSeconds <= MediumSeconds)
        LongSeconds = MediumSeconds + 1;

    const int BaselineSeconds = InputBaselineSeconds.GetInt();
    const double TickSize = sc.TickSize > 0.0f ? sc.TickSize : 0.01;
    const int DisplayMode = InputDisplayMode.GetIndex();
    const int ShowRaw = DisplayMode != 2;
    const int ShowComposite = DisplayMode != 1;
    const int ShowHistoricalVI = ShowRaw && InputShowHistoricalVI.GetYesNo();

    Score.DrawStyle = ShowComposite ? DRAWSTYLE_BAR : DRAWSTYLE_IGNORE;
    PositiveThreshold.DrawStyle = ShowComposite ? DRAWSTYLE_LINE : DRAWSTYLE_IGNORE;
    NegativeThreshold.DrawStyle = ShowComposite ? DRAWSTYLE_LINE : DRAWSTYLE_IGNORE;

    VIShort.DrawStyle = ShowHistoricalVI ? DRAWSTYLE_LINE : DRAWSTYLE_IGNORE;
    VIMedium.DrawStyle = ShowHistoricalVI ? DRAWSTYLE_LINE : DRAWSTYLE_IGNORE;
    VILong.DrawStyle = ShowHistoricalVI ? DRAWSTYLE_LINE : DRAWSTYLE_IGNORE;

    const YI_WindowMetrics Short = YI_CalculateWindowMetrics(sc, sc.Index, ShortSeconds, TickSize);
    const YI_WindowMetrics Medium = YI_CalculateWindowMetrics(sc, sc.Index, MediumSeconds, TickSize);
    const YI_WindowMetrics Long = YI_CalculateWindowMetrics(sc, sc.Index, LongSeconds, TickSize);
    const YI_BaselineMetrics Baseline = YI_CalculateBaselineMetrics(sc, sc.Index, BaselineSeconds, TickSize);

    const double VINormalizationPct = YI_Max(InputVINormalizationPct.GetFloat(), 1.0);

    const double ExpectedShortTicks = YI_ExpectedMoveTicks(
        ShortSeconds,
        LongSeconds,
        InputMinimumFlushTicks.GetFloat(),
        InputUseAdaptiveThreshold.GetYesNo(),
        InputAdaptiveSigmaMultiplier.GetFloat(),
        Baseline);

    const double ExpectedMediumTicks = YI_ExpectedMoveTicks(
        MediumSeconds,
        LongSeconds,
        InputMinimumFlushTicks.GetFloat(),
        InputUseAdaptiveThreshold.GetYesNo(),
        InputAdaptiveSigmaMultiplier.GetFloat(),
        Baseline);

    const double ExpectedLongTicks = YI_ExpectedMoveTicks(
        LongSeconds,
        LongSeconds,
        InputMinimumFlushTicks.GetFloat(),
        InputUseAdaptiveThreshold.GetYesNo(),
        InputAdaptiveSigmaMultiplier.GetFloat(),
        Baseline);

    const double FlowShort = YI_Clamp(Short.ImbalancePct / VINormalizationPct, -1.0, 1.0);
    const double FlowMedium = YI_Clamp(Medium.ImbalancePct / VINormalizationPct, -1.0, 1.0);
    const double FlowLong = YI_Clamp(Long.ImbalancePct / VINormalizationPct, -1.0, 1.0);
    const double PriorFlowShort = YI_Clamp(Short.PreviousImbalancePct / VINormalizationPct, -1.0, 1.0);

    const double PriceNormShort = YI_Clamp(Short.PriceTicks / ExpectedShortTicks, -1.0, 1.0);
    const double PriceNormMedium = YI_Clamp(Medium.PriceTicks / ExpectedMediumTicks, -1.0, 1.0);
    const double PriceNormLong = YI_Clamp(Long.PriceTicks / ExpectedLongTicks, -1.0, 1.0);

    const double DivergenceShort = YI_Clamp(PriceNormShort - FlowShort, -1.0, 1.0)
        * (0.50 + 0.50 * YI_Abs(FlowShort));
    const double DivergenceMedium = YI_Clamp(PriceNormMedium - FlowMedium, -1.0, 1.0)
        * (0.50 + 0.50 * YI_Abs(FlowMedium));
    const double DivergenceLong = YI_Clamp(PriceNormLong - FlowLong, -1.0, 1.0)
        * (0.50 + 0.50 * YI_Abs(FlowLong));

    const double AbsorptionEfficiency = YI_Clamp(
        0.55 * DivergenceShort
        + 0.30 * DivergenceMedium
        + 0.15 * DivergenceLong,
        -1.0,
        1.0);

    const double FlowSequence = YI_Clamp(
        0.70 * (FlowShort - FlowMedium)
        + 0.30 * (FlowMedium - FlowLong),
        -1.0,
        1.0);

    const double PriceSequence = YI_Clamp(
        0.70 * (PriceNormShort - PriceNormMedium)
        + 0.30 * (PriceNormMedium - PriceNormLong),
        -1.0,
        1.0);

    const double HorizonSequence = YI_Clamp(
        0.55 * FlowSequence + 0.45 * PriceSequence,
        -1.0,
        1.0);

    const double ReboundNorm = YI_Clamp(Short.ReboundFromLowTicks / ExpectedShortTicks, 0.0, 1.0);
    const double PullbackNorm = YI_Clamp(Short.PullbackFromHighTicks / ExpectedShortTicks, 0.0, 1.0);

    const double ShortPriceTurn = YI_Clamp(
        0.70 * PriceNormShort
        + 0.30 * (ReboundNorm - PullbackNorm),
        -1.0,
        1.0);

    const double FlowImprovement = YI_Clamp(
        0.60 * (FlowShort - FlowMedium)
        + 0.25 * (FlowMedium - FlowLong)
        + 0.15 * (FlowShort - PriorFlowShort),
        -1.0,
        1.0);

    const double CoreReversalSignal = YI_Clamp(
        0.35 * AbsorptionEfficiency
        + 0.25 * HorizonSequence
        + 0.25 * ShortPriceTurn
        + 0.15 * FlowImprovement,
        -1.0,
        1.0);

    const int FlushRecencySeconds = InputFlushRecencySeconds.GetInt();
    const double LowRecencyWeight = Long.LowAgeSeconds <= FlushRecencySeconds
        ? 1.0
        : YI_Clamp(2.0 - Long.LowAgeSeconds / YI_Max(FlushRecencySeconds, 1), 0.0, 1.0);
    const double HighRecencyWeight = Long.HighAgeSeconds <= FlushRecencySeconds
        ? 1.0
        : YI_Clamp(2.0 - Long.HighAgeSeconds / YI_Max(FlushRecencySeconds, 1), 0.0, 1.0);

    const double DropToLowTicks = (Long.ReferencePrice - Long.Low) / TickSize;
    const double RiseToHighTicks = (Long.High - Long.ReferencePrice) / TickSize;

    const double DownContext = YI_Clamp(
        YI_Max(
            -Long.PriceTicks / ExpectedLongTicks,
            LowRecencyWeight * DropToLowTicks / ExpectedLongTicks),
        0.0,
        1.0);

    const double UpContext = YI_Clamp(
        YI_Max(
            Long.PriceTicks / ExpectedLongTicks,
            HighRecencyWeight * RiseToHighTicks / ExpectedLongTicks),
        0.0,
        1.0);

    const double DirectionalFlowPressure = YI_Clamp(
        0.55 * FlowShort + 0.25 * FlowMedium + 0.20 * FlowLong,
        -1.0,
        1.0);

    const double DirectionalPricePressure = YI_Clamp(
        0.65 * PriceNormShort + 0.25 * PriceNormMedium + 0.10 * PriceNormLong,
        -1.0,
        1.0);

    const double ContinuationPressure = YI_Clamp(
        0.65 * DirectionalFlowPressure + 0.35 * DirectionalPricePressure,
        -1.0,
        1.0);

    const double ContextStrength = YI_Max(DownContext, UpContext);
    const double ContextScale = 0.25 + 0.75 * ContextStrength;

    const double MinimumVolume = YI_Max(InputMinimumVolume.GetInt(), 0);
    const double MinimumTrades = YI_Max(InputMinimumTrades.GetInt(), 0);

    const double VolumeGate = MinimumVolume > 0.0
        ? YI_Clamp(Short.TotalVolume / MinimumVolume, 0.0, 1.0)
        : 1.0;

    const double TradeGate = MinimumTrades > 0.0
        ? YI_Clamp(Short.Trades / MinimumTrades, 0.0, 1.0)
        : 1.0;

    const double BaseParticipationGate = sqrt(VolumeGate * TradeGate);

    double PaceRatio = 1.0;
    if (Baseline.Ready && Baseline.AverageTradesPerSecond > 0.0)
    {
        PaceRatio = (Short.Trades / YI_Max(ShortSeconds, 1))
            / Baseline.AverageTradesPerSecond;
    }

    const double PaceAccelerationNorm = YI_Clamp(Short.PaceAccelerationPct / 50.0, -1.0, 1.0);
    const double PaceActivityNorm = YI_Clamp((PaceRatio - 0.75) / 0.75, 0.0, 1.0);
    const double PaceConfidence = YI_Clamp(
        0.75
        + 0.15 * PaceActivityNorm
        + 0.10 * PaceAccelerationNorm,
        0.55,
        1.0);

    // VI already contains signed delta divided by total volume. To avoid
    // double-counting the same directional information, raw delta enters as
    // an aggression-magnitude confidence term rather than a second sign vote.
    const double BaselineVolumePerSecond = Baseline.Ready
        ? Baseline.AverageVolumePerSecond
        : YI_Max(Short.TotalVolume / YI_Max(ShortSeconds, 1), 1.0);

    const double DeltaIntensityShort = YI_Clamp(
        YI_Abs(Short.Delta)
            / YI_Max(0.15 * BaselineVolumePerSecond * ShortSeconds, 1.0),
        0.0,
        1.0);
    const double DeltaIntensityMedium = YI_Clamp(
        YI_Abs(Medium.Delta)
            / YI_Max(0.15 * BaselineVolumePerSecond * MediumSeconds, 1.0),
        0.0,
        1.0);
    const double DeltaIntensityLong = YI_Clamp(
        YI_Abs(Long.Delta)
            / YI_Max(0.15 * BaselineVolumePerSecond * LongSeconds, 1.0),
        0.0,
        1.0);

    const double MultiHorizonDeltaIntensity = YI_Clamp(
        0.55 * DeltaIntensityShort
        + 0.30 * DeltaIntensityMedium
        + 0.15 * DeltaIntensityLong,
        0.0,
        1.0);

    const double DeltaConfidence = 0.75 + 0.25 * MultiHorizonDeltaIntensity;

    const int HistoryReady = Short.HasCurrentHistory
        && Medium.HasCurrentHistory
        && Long.HasCurrentHistory;
    const int BidAskReady = Short.HasBidAskData
        && Medium.HasBidAskData
        && Long.HasBidAskData;

    double CompositeScore = 0.0;
    if (HistoryReady && BidAskReady)
    {
        CompositeScore = 100.0
            * BaseParticipationGate
            * PaceConfidence
            * DeltaConfidence
            * (0.80 * CoreReversalSignal * ContextScale
                + 0.20 * ContinuationPressure);
        CompositeScore = YI_Clamp(CompositeScore, -100.0, 100.0);
    }

    const double AbsorptionThresholdValue = InputAbsorptionThreshold.GetFloat();
    const double StrongThresholdValue = InputStrongThreshold.GetFloat();

    int CurrentStateCode = 0;
    const char* StateText = "Neutral";

    if (!HistoryReady)
    {
        StateText = "Warming up";
        CurrentStateCode = 0;
    }
    else if (!BidAskReady)
    {
        StateText = "Missing bid/ask volume";
        CurrentStateCode = 0;
    }
    else if (BaseParticipationGate < 0.50)
    {
        StateText = "Low participation";
        CurrentStateCode = 0;
    }
    else if (DownContext >= 0.35)
    {
        if (CompositeScore >= StrongThresholdValue
            && Short.PriceChange > 0.0
            && FlowImprovement > 0.0)
        {
            StateText = "Bullish reversal pressure";
            CurrentStateCode = 4;
        }
        else if (CompositeScore >= AbsorptionThresholdValue
            && AbsorptionEfficiency > 0.20)
        {
            StateText = "Bullish absorption";
            CurrentStateCode = 3;
        }
        else if (FlowImprovement > 0.10
            || ShortPriceTurn > 0.10
            || CompositeScore > 12.0)
        {
            StateText = "Sell pressure weakening";
            CurrentStateCode = 2;
        }
        else if (DirectionalFlowPressure < -0.35
            && PriceNormShort < -0.10)
        {
            StateText = "Strong sell pressure";
            CurrentStateCode = -2;
        }
    }
    else if (UpContext >= 0.35)
    {
        if (CompositeScore <= -StrongThresholdValue
            && Short.PriceChange < 0.0
            && FlowImprovement < 0.0)
        {
            StateText = "Bearish reversal pressure";
            CurrentStateCode = -4;
        }
        else if (CompositeScore <= -AbsorptionThresholdValue
            && AbsorptionEfficiency < -0.20)
        {
            StateText = "Bearish absorption";
            CurrentStateCode = -3;
        }
        else if (FlowImprovement < -0.10
            || ShortPriceTurn < -0.10
            || CompositeScore < -12.0)
        {
            StateText = "Buy pressure weakening";
            CurrentStateCode = -2;
        }
        else if (DirectionalFlowPressure > 0.35
            && PriceNormShort > 0.10)
        {
            StateText = "Strong buy pressure";
            CurrentStateCode = 2;
        }
    }
    else if (ContinuationPressure <= -0.55)
    {
        StateText = "Strong sell pressure";
        CurrentStateCode = -2;
    }
    else if (ContinuationPressure >= 0.55)
    {
        StateText = "Strong buy pressure";
        CurrentStateCode = 2;
    }

    Score[sc.Index] = static_cast<float>(CompositeScore);
    VIShort[sc.Index] = static_cast<float>(Short.ImbalancePct);
    VIMedium[sc.Index] = static_cast<float>(Medium.ImbalancePct);
    VILong[sc.Index] = static_cast<float>(Long.ImbalancePct);
    ZeroLine[sc.Index] = 0.0f;
    PositiveThreshold[sc.Index] = static_cast<float>(AbsorptionThresholdValue);
    NegativeThreshold[sc.Index] = static_cast<float>(-AbsorptionThresholdValue);
    PaceAcceleration[sc.Index] = static_cast<float>(Short.PaceAccelerationPct);
    PriceShort[sc.Index] = static_cast<float>(Short.PriceChange);
    PriceMedium[sc.Index] = static_cast<float>(Medium.PriceChange);
    PriceLong[sc.Index] = static_cast<float>(Long.PriceChange);
    DeltaShort[sc.Index] = static_cast<float>(Short.Delta);
    DeltaMedium[sc.Index] = static_cast<float>(Medium.Delta);
    DeltaLong[sc.Index] = static_cast<float>(Long.Delta);
    TradesShort[sc.Index] = static_cast<float>(Short.Trades);
    StateCode[sc.Index] = static_cast<float>(CurrentStateCode);
    AbsorptionComponent[sc.Index] = static_cast<float>(100.0 * AbsorptionEfficiency);
    DownFlushContext[sc.Index] = static_cast<float>(100.0 * DownContext);
    UpFlushContext[sc.Index] = static_cast<float>(100.0 * UpContext);
    DeltaIntensity[sc.Index] = static_cast<float>(100.0 * MultiHorizonDeltaIntensity);

    const unsigned int PositiveColor = RGB(0, 200, 255);
    const unsigned int BullishStateColor = RGB(0, 220, 125);
    const unsigned int NegativeColor = RGB(255, 65, 65);
    const unsigned int NeutralColor = RGB(185, 190, 195);

    if (CompositeScore > 0.5)
        Score.DataColor[sc.Index] = CurrentStateCode >= 3 ? BullishStateColor : PositiveColor;
    else if (CompositeScore < -0.5)
        Score.DataColor[sc.Index] = NegativeColor;
    else
        Score.DataColor[sc.Index] = NeutralColor;

    VIShort.DataColor[sc.Index] = Short.ImbalancePct >= 0.0 ? VIShort.PrimaryColor : VIShort.SecondaryColor;
    VIMedium.DataColor[sc.Index] = Medium.ImbalancePct >= 0.0 ? VIMedium.PrimaryColor : VIMedium.SecondaryColor;
    VILong.DataColor[sc.Index] = Long.ImbalancePct >= 0.0 ? VILong.PrimaryColor : VILong.SecondaryColor;

    if (sc.Index == sc.ArraySize - 1)
    {
        const int DrawingBase = 9000000 + (sc.StudyGraphInstanceID % 10000) * 100;

        SCString Text;
        SCString DeltaShortText;
        SCString DeltaMediumText;
        SCString DeltaLongText;
        SCString ShortWindowLabel;
        SCString MediumWindowLabel;
        SCString LongWindowLabel;
        YI_FormatSignedCompactNumber(Short.Delta, DeltaShortText);
        YI_FormatSignedCompactNumber(Medium.Delta, DeltaMediumText);
        YI_FormatSignedCompactNumber(Long.Delta, DeltaLongText);
        YI_FormatWindowLabel(ShortSeconds, ShortWindowLabel);
        YI_FormatWindowLabel(MediumSeconds, MediumWindowLabel);
        YI_FormatWindowLabel(LongSeconds, LongWindowLabel);

        if (ShowRaw && ShowComposite)
        {
            Text.Format(
                "Abs %+.0f | %s\n"
                "VI %s %+.0f | %s %+.0f | %s %+.0f | Pace %+.0f%%\n"
                "Px %+.2f / %+.2f / %+.2f | Delta %s / %s / %s",
                CompositeScore,
                StateText,
                ShortWindowLabel.GetChars(),
                Short.ImbalancePct,
                MediumWindowLabel.GetChars(),
                Medium.ImbalancePct,
                LongWindowLabel.GetChars(),
                Long.ImbalancePct,
                Short.PaceAccelerationPct,
                Short.PriceChange,
                Medium.PriceChange,
                Long.PriceChange,
                DeltaShortText.GetChars(),
                DeltaMediumText.GetChars(),
                DeltaLongText.GetChars());
        }
        else if (ShowRaw)
        {
            Text.Format(
                "VI %s %+.0f | %s %+.0f | %s %+.0f\n"
                "Pace %+.0f%% | Px %+.2f / %+.2f / %+.2f\n"
                "Delta %s / %s / %s",
                ShortWindowLabel.GetChars(),
                Short.ImbalancePct,
                MediumWindowLabel.GetChars(),
                Medium.ImbalancePct,
                LongWindowLabel.GetChars(),
                Long.ImbalancePct,
                Short.PaceAccelerationPct,
                Short.PriceChange,
                Medium.PriceChange,
                Long.PriceChange,
                DeltaShortText.GetChars(),
                DeltaMediumText.GetChars(),
                DeltaLongText.GetChars());
        }
        else
        {
            Text.Format(
                "Absorption %+.0f\n%s\nPace %+.0f%% | Px %+.2f",
                CompositeScore,
                StateText,
                Short.PaceAccelerationPct,
                Short.PriceChange);
        }

        unsigned int TextColor = NeutralColor;
        if (CurrentStateCode >= 3)
            TextColor = BullishStateColor;
        else if (CompositeScore > 0.5)
            TextColor = PositiveColor;
        else if (CompositeScore < -0.5 || CurrentStateCode < 0)
            TextColor = NegativeColor;

        YI_DrawTextPanel(
            sc,
            DrawingBase,
            InputTextHorizontalPosition.GetInt(),
            InputTextVerticalPosition.GetFloat(),
            InputTextFontSize.GetInt(),
            TextColor,
            Text);

        if (InputShowGauges.GetYesNo())
        {
            const int GaugeCenterX = InputGaugeCenterX.GetInt();
            const int GaugeHalfWidth = InputGaugeHalfWidth.GetInt();

            if (ShowRaw)
            {
                YI_DrawGauge(sc, DrawingBase + 10, GaugeCenterX, GaugeHalfWidth,
                    62.0f, 69.0f, Short.ImbalancePct,
                    PositiveColor, NegativeColor, NeutralColor);
                YI_DrawGauge(sc, DrawingBase + 20, GaugeCenterX, GaugeHalfWidth,
                    49.0f, 56.0f, Medium.ImbalancePct,
                    PositiveColor, NegativeColor, NeutralColor);
                YI_DrawGauge(sc, DrawingBase + 30, GaugeCenterX, GaugeHalfWidth,
                    36.0f, 43.0f, Long.ImbalancePct,
                    PositiveColor, NegativeColor, NeutralColor);
            }

            if (ShowComposite)
            {
                YI_DrawGauge(sc, DrawingBase + 40, GaugeCenterX, GaugeHalfWidth,
                    16.0f, 25.0f, CompositeScore,
                    BullishStateColor, NegativeColor, NeutralColor);
            }
        }

        if (InputEnableAlerts.GetYesNo() && !sc.IsFullRecalculation)
        {
            const double AlertThreshold = InputAlertThreshold.GetFloat();
            int& LastAlertDirection = sc.GetPersistentInt(1);
            SCDateTime& LastAlertDateTime = sc.GetPersistentSCDateTime(1);

            SCDateTime CurrentAlertDateTime = sc.LatestDateTimeForLastBar;
            if (CurrentAlertDateTime.GetAsDouble() == 0.0)
                CurrentAlertDateTime = sc.BaseDateTimeIn[sc.Index];

            int CooldownExpired = 1;
            if (LastAlertDateTime.GetAsDouble() != 0.0)
            {
                SCDateTime NextAllowedAlert = LastAlertDateTime;
                NextAllowedAlert += SCDateTime::SECONDS(InputAlertCooldownSeconds.GetInt());
                CooldownExpired = CurrentAlertDateTime >= NextAllowedAlert;
            }

            if (CompositeScore < AlertThreshold - 5.0
                && CompositeScore > -AlertThreshold + 5.0)
            {
                LastAlertDirection = 0;
            }

            if (CooldownExpired
                && CompositeScore >= AlertThreshold
                && LastAlertDirection != 1)
            {
                SCString Message;
                Message.Format("Flush reversal: %s | Score %+.0f", StateText, CompositeScore);
                sc.SetAlert(InputAlertNumber.GetInt(), Message);
                LastAlertDirection = 1;
                LastAlertDateTime = CurrentAlertDateTime;
            }
            else if (CooldownExpired
                && CompositeScore <= -AlertThreshold
                && LastAlertDirection != -1)
            {
                SCString Message;
                Message.Format("Bearish mirror: %s | Score %+.0f", StateText, CompositeScore);
                sc.SetAlert(InputAlertNumber.GetInt(), Message);
                LastAlertDirection = -1;
                LastAlertDateTime = CurrentAlertDateTime;
            }
        }
    }
}

namespace
{
    struct YI_FastWindowMetrics
    {
        double AskVolume;
        double BidVolume;
        double TotalVolume;
        double Delta;
        double ImbalancePct;
        double Trades;

        double PriceChange;
        double PriceTicks;
        double ReferencePrice;
        double Low;
        double High;
        double ReboundFromLowTicks;
        double PullbackFromHighTicks;
        double LowAgeSeconds;
        double HighAgeSeconds;

        double CoverageSeconds;
        double CoverageRatio;

        int StartIndex;
        int LowIndex;
        int HighIndex;
        int HasBidAskData;
    };

    struct YI_FastBundle
    {
        YI_FastWindowMetrics Short;
        YI_FastWindowMetrics Medium;
        YI_FastWindowMetrics Long;

        double PreviousShortAskVolume;
        double PreviousShortBidVolume;
        double PreviousShortTrades;
        double PreviousShortCoverageSeconds;
        double PreviousShortImbalancePct;
        double PaceAccelerationPct;
        int PreviousShortReady;
    };

    void YI_InitializeFastWindow(
        YI_FastWindowMetrics& Window,
        SCStudyInterfaceRef sc,
        const int Index)
    {
        Window = {};
        Window.StartIndex = Index;
        Window.LowIndex = Index;
        Window.HighIndex = Index;
        Window.Low = sc.Low[Index];
        Window.High = sc.High[Index];
    }

    int YI_IsFirstBarAfterGap(
        SCStudyInterfaceRef sc,
        const int BarIndex,
        const int SessionGapSeconds)
    {
        if (BarIndex <= 0)
            return 0;

        const double GapSeconds = YI_DateTimeDifferenceInSeconds(
            sc.BaseDateTimeIn[BarIndex],
            sc.BaseDateTimeIn[BarIndex - 1]);

        return GapSeconds >= SessionGapSeconds;
    }

    void YI_AddBarToFastWindow(
        SCStudyInterfaceRef sc,
        const int BarIndex,
        const int IncludeFlow,
        YI_FastWindowMetrics& Window)
    {
        Window.StartIndex = BarIndex;

        if (IncludeFlow)
        {
            Window.AskVolume += sc.BaseData[SC_ASKVOL][BarIndex];
            Window.BidVolume += sc.BaseData[SC_BIDVOL][BarIndex];
            Window.Trades += sc.NumberOfTrades[BarIndex];
        }

        if (sc.Low[BarIndex] < Window.Low)
        {
            Window.Low = sc.Low[BarIndex];
            Window.LowIndex = BarIndex;
        }

        if (sc.High[BarIndex] > Window.High)
        {
            Window.High = sc.High[BarIndex];
            Window.HighIndex = BarIndex;
        }
    }

    void YI_FinalizeFastWindow(
        SCStudyInterfaceRef sc,
        const int Index,
        const int WindowSeconds,
        const double TickSize,
        const double EstimatedBarSeconds,
        YI_FastWindowMetrics& Window)
    {
        const SCDateTime CurrentDateTime = sc.BaseDateTimeIn[Index];

        Window.TotalVolume = Window.AskVolume + Window.BidVolume;
        Window.Delta = Window.AskVolume - Window.BidVolume;
        Window.ImbalancePct = 100.0 * Window.Delta / YI_Max(Window.TotalVolume, 1.0);

        Window.ReferencePrice = sc.Open[Window.StartIndex];
        Window.PriceChange = sc.Close[Index] - Window.ReferencePrice;
        Window.PriceTicks = Window.PriceChange / TickSize;

        Window.ReboundFromLowTicks = (sc.Close[Index] - Window.Low) / TickSize;
        Window.PullbackFromHighTicks = (Window.High - sc.Close[Index]) / TickSize;
        Window.LowAgeSeconds = YI_DateTimeDifferenceInSeconds(
            CurrentDateTime,
            sc.BaseDateTimeIn[Window.LowIndex]);
        Window.HighAgeSeconds = YI_DateTimeDifferenceInSeconds(
            CurrentDateTime,
            sc.BaseDateTimeIn[Window.HighIndex]);

        Window.CoverageSeconds = YI_DateTimeDifferenceInSeconds(
            CurrentDateTime,
            sc.BaseDateTimeIn[Window.StartIndex]) + EstimatedBarSeconds;
        Window.CoverageSeconds = YI_Clamp(Window.CoverageSeconds, 0.0, WindowSeconds);
        Window.CoverageRatio = YI_Clamp(
            Window.CoverageSeconds / YI_Max(static_cast<double>(WindowSeconds), 1.0),
            0.0,
            1.0);
        Window.HasBidAskData = Window.TotalVolume > 0.0;
    }

    YI_FastBundle YI_CalculateFastBundle(
        SCStudyInterfaceRef sc,
        const int Index,
        const int ShortSeconds,
        const int MediumSeconds,
        const int LongSeconds,
        const double TickSize,
        const int ExcludeFirstBarAfterGap,
        const int SessionGapSeconds)
    {
        YI_FastBundle Result = {};
        YI_InitializeFastWindow(Result.Short, sc, Index);
        YI_InitializeFastWindow(Result.Medium, sc, Index);
        YI_InitializeFastWindow(Result.Long, sc, Index);

        const SCDateTime CurrentDateTime = sc.BaseDateTimeIn[Index];
        const int MaximumLookbackSeconds = LongSeconds > 2 * ShortSeconds
            ? LongSeconds
            : 2 * ShortSeconds;

        double EstimatedBarSeconds = 1.0;
        if (Index > 0)
        {
            const double CandidateSeconds = YI_DateTimeDifferenceInSeconds(
                sc.BaseDateTimeIn[Index],
                sc.BaseDateTimeIn[Index - 1]);
            if (CandidateSeconds > 0.0 && CandidateSeconds <= 60.0)
                EstimatedBarSeconds = CandidateSeconds;
        }

        int PreviousShortOldestIndex = -1;
        int PreviousShortNewestIndex = -1;

        for (int BarIndex = Index; BarIndex >= 0; --BarIndex)
        {
            const double AgeSeconds = YI_DateTimeDifferenceInSeconds(
                CurrentDateTime,
                sc.BaseDateTimeIn[BarIndex]);

            if (AgeSeconds >= MaximumLookbackSeconds)
                break;

            const int IsFirstAfterGap = YI_IsFirstBarAfterGap(
                sc,
                BarIndex,
                SessionGapSeconds);
            const int IncludeFlow = !(ExcludeFirstBarAfterGap && IsFirstAfterGap);

            if (AgeSeconds < LongSeconds)
                YI_AddBarToFastWindow(sc, BarIndex, IncludeFlow, Result.Long);

            if (AgeSeconds < MediumSeconds)
                YI_AddBarToFastWindow(sc, BarIndex, IncludeFlow, Result.Medium);

            if (AgeSeconds < ShortSeconds)
            {
                YI_AddBarToFastWindow(sc, BarIndex, IncludeFlow, Result.Short);
            }
            else if (AgeSeconds < 2 * ShortSeconds)
            {
                if (IncludeFlow)
                {
                    Result.PreviousShortAskVolume += sc.BaseData[SC_ASKVOL][BarIndex];
                    Result.PreviousShortBidVolume += sc.BaseData[SC_BIDVOL][BarIndex];
                    Result.PreviousShortTrades += sc.NumberOfTrades[BarIndex];
                }

                if (PreviousShortNewestIndex < 0)
                    PreviousShortNewestIndex = BarIndex;
                PreviousShortOldestIndex = BarIndex;
            }
        }

        YI_FinalizeFastWindow(
            sc,
            Index,
            ShortSeconds,
            TickSize,
            EstimatedBarSeconds,
            Result.Short);
        YI_FinalizeFastWindow(
            sc,
            Index,
            MediumSeconds,
            TickSize,
            EstimatedBarSeconds,
            Result.Medium);
        YI_FinalizeFastWindow(
            sc,
            Index,
            LongSeconds,
            TickSize,
            EstimatedBarSeconds,
            Result.Long);

        const double PreviousShortTotalVolume = Result.PreviousShortAskVolume
            + Result.PreviousShortBidVolume;
        Result.PreviousShortImbalancePct = 100.0
            * (Result.PreviousShortAskVolume - Result.PreviousShortBidVolume)
            / YI_Max(PreviousShortTotalVolume, 1.0);

        if (PreviousShortNewestIndex >= 0 && PreviousShortOldestIndex >= 0)
        {
            Result.PreviousShortCoverageSeconds = YI_DateTimeDifferenceInSeconds(
                sc.BaseDateTimeIn[PreviousShortNewestIndex],
                sc.BaseDateTimeIn[PreviousShortOldestIndex]) + EstimatedBarSeconds;
            Result.PreviousShortCoverageSeconds = YI_Clamp(
                Result.PreviousShortCoverageSeconds,
                0.0,
                ShortSeconds);
        }

        Result.PreviousShortReady = Result.PreviousShortCoverageSeconds
            >= 0.75 * ShortSeconds;

        if (Result.PreviousShortReady && Result.Short.CoverageSeconds > 0.0)
        {
            const double CurrentRate = Result.Short.Trades
                / YI_Max(Result.Short.CoverageSeconds, 1.0);
            const double PreviousRate = Result.PreviousShortTrades
                / YI_Max(Result.PreviousShortCoverageSeconds, 1.0);

            Result.PaceAccelerationPct = 100.0
                * (CurrentRate - PreviousRate)
                / YI_Max(PreviousRate, 0.10);
        }

        return Result;
    }

    double YI_FastExpectedMoveTicks(
        const int WindowSeconds,
        const int LongSeconds,
        const double CoverageSeconds,
        const double LongFlushTicks)
    {
        const double EffectiveSeconds = YI_Clamp(
            CoverageSeconds,
            1.0,
            WindowSeconds);
        const double TimeRatio = EffectiveSeconds
            / YI_Max(static_cast<double>(LongSeconds), 1.0);

        return YI_Max(1.0, LongFlushTicks * sqrt(YI_Max(TimeRatio, 0.0001)));
    }

    double YI_WeightedAverage3(
        const double Value1,
        const double Weight1,
        const double Value2,
        const double Weight2,
        const double Value3,
        const double Weight3)
    {
        const double TotalWeight = Weight1 + Weight2 + Weight3;
        if (TotalWeight <= 0.0)
            return 0.0;

        return (Value1 * Weight1 + Value2 * Weight2 + Value3 * Weight3)
            / TotalWeight;
    }

    void YI_FormatWindowLabelWithCoverage(
        const int Seconds,
        const double CoverageRatio,
        SCString& Output)
    {
        SCString BaseLabel;
        YI_FormatWindowLabel(Seconds, BaseLabel);

        if (CoverageRatio >= 0.95)
            Output = BaseLabel;
        else
            Output.Format("%s*", BaseLabel.GetChars());
    }
}

SCSFExport scsf_FastScalpFlushReversalAbsorption(SCStudyInterfaceRef sc)
{
    SCSubgraphRef Score = sc.Subgraph[0];
    SCSubgraphRef VIShort = sc.Subgraph[1];
    SCSubgraphRef VIMedium = sc.Subgraph[2];
    SCSubgraphRef VILong = sc.Subgraph[3];
    SCSubgraphRef ZeroLine = sc.Subgraph[4];
    SCSubgraphRef PositiveThreshold = sc.Subgraph[5];
    SCSubgraphRef NegativeThreshold = sc.Subgraph[6];
    SCSubgraphRef PaceAcceleration = sc.Subgraph[7];
    SCSubgraphRef PriceShort = sc.Subgraph[8];
    SCSubgraphRef PriceMedium = sc.Subgraph[9];
    SCSubgraphRef PriceLong = sc.Subgraph[10];
    SCSubgraphRef DeltaShort = sc.Subgraph[11];
    SCSubgraphRef DeltaMedium = sc.Subgraph[12];
    SCSubgraphRef DeltaLong = sc.Subgraph[13];
    SCSubgraphRef CoverageConfidence = sc.Subgraph[14];
    SCSubgraphRef StateCode = sc.Subgraph[15];

    SCInputRef InputShortSeconds = sc.Input[0];
    SCInputRef InputMediumSeconds = sc.Input[1];
    SCInputRef InputLongSeconds = sc.Input[2];
    SCInputRef InputMinimumStartupSeconds = sc.Input[3];
    SCInputRef InputMinimumVolume = sc.Input[4];
    SCInputRef InputMinimumTrades = sc.Input[5];
    SCInputRef InputVINormalizationPct = sc.Input[6];
    SCInputRef InputMinimumFlushTicks = sc.Input[7];
    SCInputRef InputMinimumFlushBasisPoints = sc.Input[8];
    SCInputRef InputFlushRecencySeconds = sc.Input[9];
    SCInputRef InputAbsorptionThreshold = sc.Input[10];
    SCInputRef InputStrongThreshold = sc.Input[11];
    SCInputRef InputExcludeFirstBarAfterGap = sc.Input[12];
    SCInputRef InputSessionGapSeconds = sc.Input[13];
    SCInputRef InputDetailedHistorySeconds = sc.Input[14];
    SCInputRef InputShowHistoricalVI = sc.Input[15];
    SCInputRef InputShowGauges = sc.Input[16];
    SCInputRef InputTextHorizontalPosition = sc.Input[17];
    SCInputRef InputTextVerticalPosition = sc.Input[18];
    SCInputRef InputTextFontSize = sc.Input[19];
    SCInputRef InputGaugeCenterX = sc.Input[20];
    SCInputRef InputGaugeHalfWidth = sc.Input[21];
    SCInputRef InputEnableAlerts = sc.Input[22];
    SCInputRef InputAlertThreshold = sc.Input[23];
    SCInputRef InputAlertNumber = sc.Input[24];
    SCInputRef InputAlertCooldownSeconds = sc.Input[25];

    if (sc.SetDefaults)
    {
        sc.GraphName = "Fast Scalp Flush Reversal / Absorption";
        sc.StudyDescription = "Fast-start, time-based 20s/1m/2m volume imbalance and flush-reversal score for 5-second equity charts.";
        sc.AutoLoop = 1;
        sc.GraphRegion = 1;
        sc.UpdateAlways = 1;
        sc.FreeDLL = 0;
        sc.ScaleRangeType = SCALE_USERDEFINED;
        sc.ScaleRangeTop = 100.0f;
        sc.ScaleRangeBottom = -100.0f;
        sc.ValueFormat = 1;

        Score.Name = "Fast Absorption / Reversal Score";
        Score.DrawStyle = DRAWSTYLE_BAR;
        Score.PrimaryColor = RGB(0, 210, 120);
        Score.SecondaryColor = RGB(255, 55, 55);
        Score.SecondaryColorUsed = 1;
        Score.LineWidth = 3;
        Score.DrawZeros = 0;

        VIShort.Name = "Fast VI Short";
        VIShort.DrawStyle = DRAWSTYLE_IGNORE;
        VIShort.PrimaryColor = RGB(0, 200, 255);
        VIShort.SecondaryColor = RGB(255, 70, 70);
        VIShort.SecondaryColorUsed = 1;
        VIShort.LineWidth = 2;
        VIShort.DrawZeros = 0;

        VIMedium.Name = "Fast VI Medium";
        VIMedium.DrawStyle = DRAWSTYLE_IGNORE;
        VIMedium.PrimaryColor = RGB(70, 185, 255);
        VIMedium.SecondaryColor = RGB(235, 90, 90);
        VIMedium.SecondaryColorUsed = 1;
        VIMedium.LineWidth = 1;
        VIMedium.DrawZeros = 0;

        VILong.Name = "Fast VI Long";
        VILong.DrawStyle = DRAWSTYLE_IGNORE;
        VILong.PrimaryColor = RGB(100, 165, 235);
        VILong.SecondaryColor = RGB(210, 110, 110);
        VILong.SecondaryColorUsed = 1;
        VILong.LineWidth = 1;
        VILong.DrawZeros = 0;

        ZeroLine.Name = "Zero";
        ZeroLine.DrawStyle = DRAWSTYLE_LINE;
        ZeroLine.PrimaryColor = RGB(125, 130, 135);
        ZeroLine.LineWidth = 1;
        ZeroLine.LineStyle = LINESTYLE_SOLID;
        ZeroLine.DrawZeros = 1;

        PositiveThreshold.Name = "Bullish Threshold";
        PositiveThreshold.DrawStyle = DRAWSTYLE_LINE;
        PositiveThreshold.PrimaryColor = RGB(0, 125, 85);
        PositiveThreshold.LineWidth = 1;
        PositiveThreshold.LineStyle = LINESTYLE_DASH;

        NegativeThreshold.Name = "Bearish Threshold";
        NegativeThreshold.DrawStyle = DRAWSTYLE_LINE;
        NegativeThreshold.PrimaryColor = RGB(145, 45, 45);
        NegativeThreshold.LineWidth = 1;
        NegativeThreshold.LineStyle = LINESTYLE_DASH;

        PaceAcceleration.Name = "Fast Pace Acceleration Percent";
        PaceAcceleration.DrawStyle = DRAWSTYLE_IGNORE;
        PriceShort.Name = "Fast Price Change Short";
        PriceShort.DrawStyle = DRAWSTYLE_IGNORE;
        PriceMedium.Name = "Fast Price Change Medium";
        PriceMedium.DrawStyle = DRAWSTYLE_IGNORE;
        PriceLong.Name = "Fast Price Change Long";
        PriceLong.DrawStyle = DRAWSTYLE_IGNORE;
        DeltaShort.Name = "Fast Delta Short";
        DeltaShort.DrawStyle = DRAWSTYLE_IGNORE;
        DeltaMedium.Name = "Fast Delta Medium";
        DeltaMedium.DrawStyle = DRAWSTYLE_IGNORE;
        DeltaLong.Name = "Fast Delta Long";
        DeltaLong.DrawStyle = DRAWSTYLE_IGNORE;
        CoverageConfidence.Name = "Fast Readiness Percent";
        CoverageConfidence.DrawStyle = DRAWSTYLE_IGNORE;
        StateCode.Name = "Fast State Code";
        StateCode.DrawStyle = DRAWSTYLE_IGNORE;

        InputShortSeconds.Name = "Short Window Seconds";
        InputShortSeconds.SetInt(20);
        InputShortSeconds.SetIntLimits(5, 300);

        InputMediumSeconds.Name = "Medium Window Seconds";
        InputMediumSeconds.SetInt(60);
        InputMediumSeconds.SetIntLimits(10, 600);

        InputLongSeconds.Name = "Long Window Seconds";
        InputLongSeconds.SetInt(120);
        InputLongSeconds.SetIntLimits(20, 900);

        InputMinimumStartupSeconds.Name = "Minimum Seconds Before Early Score";
        InputMinimumStartupSeconds.SetInt(15);
        InputMinimumStartupSeconds.SetIntLimits(5, 120);

        InputMinimumVolume.Name = "Minimum Bid+Ask Volume In Full Short Window";
        InputMinimumVolume.SetInt(1000);
        InputMinimumVolume.SetIntLimits(0, 2000000000);

        InputMinimumTrades.Name = "Minimum Trades In Full Short Window";
        InputMinimumTrades.SetInt(15);
        InputMinimumTrades.SetIntLimits(0, 10000000);

        InputVINormalizationPct.Name = "Meaningful VI Percent";
        InputVINormalizationPct.SetFloat(20.0f);
        InputVINormalizationPct.SetFloatLimits(1.0f, 100.0f);

        InputMinimumFlushTicks.Name = "Minimum Long-Window Flush Ticks";
        InputMinimumFlushTicks.SetFloat(6.0f);
        InputMinimumFlushTicks.SetFloatLimits(1.0f, 1000.0f);

        InputMinimumFlushBasisPoints.Name = "Minimum Long-Window Flush Basis Points";
        InputMinimumFlushBasisPoints.SetFloat(8.0f);
        InputMinimumFlushBasisPoints.SetFloatLimits(0.0f, 1000.0f);

        InputFlushRecencySeconds.Name = "Flush High/Low Recency Seconds";
        InputFlushRecencySeconds.SetInt(45);
        InputFlushRecencySeconds.SetIntLimits(5, 600);

        InputAbsorptionThreshold.Name = "Absorption State Threshold";
        InputAbsorptionThreshold.SetFloat(35.0f);
        InputAbsorptionThreshold.SetFloatLimits(1.0f, 99.0f);

        InputStrongThreshold.Name = "Strong Reversal Threshold";
        InputStrongThreshold.SetFloat(60.0f);
        InputStrongThreshold.SetFloatLimits(1.0f, 100.0f);

        InputExcludeFirstBarAfterGap.Name = "Exclude First Bar After Session Gap From Flow";
        InputExcludeFirstBarAfterGap.SetYesNo(1);

        InputSessionGapSeconds.Name = "Session Gap Detection Seconds";
        InputSessionGapSeconds.SetInt(600);
        InputSessionGapSeconds.SetIntLimits(60, 86400);

        InputDetailedHistorySeconds.Name = "Historical Score Display Seconds";
        InputDetailedHistorySeconds.SetInt(60);
        InputDetailedHistorySeconds.SetIntLimits(0, 3600);

        InputShowHistoricalVI.Name = "Show Historical VI Lines";
        InputShowHistoricalVI.SetYesNo(0);

        InputShowGauges.Name = "Show Compact Gauges";
        InputShowGauges.SetYesNo(1);

        InputTextHorizontalPosition.Name = "Text Horizontal Position (1-149)";
        InputTextHorizontalPosition.SetInt(3);
        InputTextHorizontalPosition.SetIntLimits(1, 149);

        InputTextVerticalPosition.Name = "Text Vertical Position Percent";
        InputTextVerticalPosition.SetFloat(97.0f);
        InputTextVerticalPosition.SetFloatLimits(1.0f, 100.0f);

        InputTextFontSize.Name = "Text Font Size";
        InputTextFontSize.SetInt(8);
        InputTextFontSize.SetIntLimits(6, 24);

        InputGaugeCenterX.Name = "Gauge Center Horizontal Position (1-149)";
        InputGaugeCenterX.SetInt(128);
        InputGaugeCenterX.SetIntLimits(10, 140);

        InputGaugeHalfWidth.Name = "Gauge Half Width";
        InputGaugeHalfWidth.SetInt(17);
        InputGaugeHalfWidth.SetIntLimits(4, 50);

        InputEnableAlerts.Name = "Enable Score Alerts";
        InputEnableAlerts.SetYesNo(0);

        InputAlertThreshold.Name = "Alert Absolute Score Threshold";
        InputAlertThreshold.SetFloat(60.0f);
        InputAlertThreshold.SetFloatLimits(1.0f, 100.0f);

        InputAlertNumber.Name = "Alert Sound Number";
        InputAlertNumber.SetInt(1);
        InputAlertNumber.SetIntLimits(1, 150);

        InputAlertCooldownSeconds.Name = "Alert Cooldown Seconds";
        InputAlertCooldownSeconds.SetInt(20);
        InputAlertCooldownSeconds.SetIntLimits(0, 3600);

        return;
    }

    int ShortSeconds = InputShortSeconds.GetInt();
    int MediumSeconds = InputMediumSeconds.GetInt();
    int LongSeconds = InputLongSeconds.GetInt();

    if (MediumSeconds <= ShortSeconds)
        MediumSeconds = ShortSeconds + 1;
    if (LongSeconds <= MediumSeconds)
        LongSeconds = MediumSeconds + 1;

    const double TickSize = sc.TickSize > 0.0f ? sc.TickSize : 0.01;
    const double AbsorptionThresholdValue = InputAbsorptionThreshold.GetFloat();
    const int ShowHistoricalVI = InputShowHistoricalVI.GetYesNo();

    VIShort.DrawStyle = ShowHistoricalVI ? DRAWSTYLE_LINE : DRAWSTYLE_IGNORE;
    VIMedium.DrawStyle = ShowHistoricalVI ? DRAWSTYLE_LINE : DRAWSTYLE_IGNORE;
    VILong.DrawStyle = ShowHistoricalVI ? DRAWSTYLE_LINE : DRAWSTYLE_IGNORE;

    ZeroLine[sc.Index] = 0.0f;
    PositiveThreshold[sc.Index] = static_cast<float>(AbsorptionThresholdValue);
    NegativeThreshold[sc.Index] = static_cast<float>(-AbsorptionThresholdValue);

    // Only perform the detailed rolling calculation for the most recent
    // display interval plus the long lookback. Older bars are intentionally
    // skipped so a full-day 5-second chart remains light.
    if (sc.Index < sc.ArraySize - 1 && InputDetailedHistorySeconds.GetInt() >= 0)
    {
        const double AgeFromLastBar = YI_DateTimeDifferenceInSeconds(
            sc.BaseDateTimeIn[sc.ArraySize - 1],
            sc.BaseDateTimeIn[sc.Index]);
        const double MaximumDetailedAge = InputDetailedHistorySeconds.GetInt()
            + LongSeconds;

        if (AgeFromLastBar > MaximumDetailedAge)
        {
            Score[sc.Index] = 0.0f;
            VIShort[sc.Index] = 0.0f;
            VIMedium[sc.Index] = 0.0f;
            VILong[sc.Index] = 0.0f;
            PaceAcceleration[sc.Index] = 0.0f;
            PriceShort[sc.Index] = 0.0f;
            PriceMedium[sc.Index] = 0.0f;
            PriceLong[sc.Index] = 0.0f;
            DeltaShort[sc.Index] = 0.0f;
            DeltaMedium[sc.Index] = 0.0f;
            DeltaLong[sc.Index] = 0.0f;
            CoverageConfidence[sc.Index] = 0.0f;
            StateCode[sc.Index] = 0.0f;
            return;
        }
    }

    const YI_FastBundle Metrics = YI_CalculateFastBundle(
        sc,
        sc.Index,
        ShortSeconds,
        MediumSeconds,
        LongSeconds,
        TickSize,
        InputExcludeFirstBarAfterGap.GetYesNo(),
        InputSessionGapSeconds.GetInt());

    const YI_FastWindowMetrics& Short = Metrics.Short;
    const YI_FastWindowMetrics& Medium = Metrics.Medium;
    const YI_FastWindowMetrics& Long = Metrics.Long;

    const double VINormalizationPct = YI_Max(InputVINormalizationPct.GetFloat(), 1.0);
    const double PriceBasisPointTicks = YI_Abs(sc.Close[sc.Index])
        * InputMinimumFlushBasisPoints.GetFloat()
        / 10000.0
        / TickSize;
    const double LongFlushTicks = YI_Max(
        InputMinimumFlushTicks.GetFloat(),
        PriceBasisPointTicks);

    const double ExpectedShortTicks = YI_FastExpectedMoveTicks(
        ShortSeconds,
        LongSeconds,
        Short.CoverageSeconds,
        LongFlushTicks);
    const double ExpectedMediumTicks = YI_FastExpectedMoveTicks(
        MediumSeconds,
        LongSeconds,
        Medium.CoverageSeconds,
        LongFlushTicks);
    const double ExpectedLongTicks = YI_FastExpectedMoveTicks(
        LongSeconds,
        LongSeconds,
        Long.CoverageSeconds,
        LongFlushTicks);

    const double FlowShort = YI_Clamp(Short.ImbalancePct / VINormalizationPct, -1.0, 1.0);
    const double FlowMedium = YI_Clamp(Medium.ImbalancePct / VINormalizationPct, -1.0, 1.0);
    const double FlowLong = YI_Clamp(Long.ImbalancePct / VINormalizationPct, -1.0, 1.0);
    const double PreviousFlowShort = YI_Clamp(
        Metrics.PreviousShortImbalancePct / VINormalizationPct,
        -1.0,
        1.0);

    const double PriceNormShort = YI_Clamp(Short.PriceTicks / ExpectedShortTicks, -1.0, 1.0);
    const double PriceNormMedium = YI_Clamp(Medium.PriceTicks / ExpectedMediumTicks, -1.0, 1.0);
    const double PriceNormLong = YI_Clamp(Long.PriceTicks / ExpectedLongTicks, -1.0, 1.0);

    const double DivergenceShort = YI_Clamp(PriceNormShort - FlowShort, -1.0, 1.0)
        * (0.50 + 0.50 * YI_Abs(FlowShort));
    const double DivergenceMedium = YI_Clamp(PriceNormMedium - FlowMedium, -1.0, 1.0)
        * (0.50 + 0.50 * YI_Abs(FlowMedium));
    const double DivergenceLong = YI_Clamp(PriceNormLong - FlowLong, -1.0, 1.0)
        * (0.50 + 0.50 * YI_Abs(FlowLong));

    const double AbsorptionEfficiency = YI_Clamp(
        YI_WeightedAverage3(
            DivergenceShort,
            0.55 * Short.CoverageRatio,
            DivergenceMedium,
            0.30 * Medium.CoverageRatio,
            DivergenceLong,
            0.15 * Long.CoverageRatio),
        -1.0,
        1.0);

    const double SequenceSMAvailability = YI_Clamp(
        (Medium.CoverageSeconds - Short.CoverageSeconds)
            / YI_Max(static_cast<double>(ShortSeconds), 1.0),
        0.0,
        1.0);
    const double SequenceMLAvailability = YI_Clamp(
        (Long.CoverageSeconds - Medium.CoverageSeconds)
            / YI_Max(static_cast<double>(MediumSeconds), 1.0),
        0.0,
        1.0);

    const double SequenceAvailability = YI_Clamp(
        0.65 * SequenceSMAvailability + 0.35 * SequenceMLAvailability,
        0.0,
        1.0);

    const double FlowSequence = YI_Clamp(
        YI_WeightedAverage3(
            FlowShort - FlowMedium,
            0.70 * SequenceSMAvailability,
            FlowMedium - FlowLong,
            0.30 * SequenceMLAvailability,
            0.0,
            0.0),
        -1.0,
        1.0);

    const double PriceSequence = YI_Clamp(
        YI_WeightedAverage3(
            PriceNormShort - PriceNormMedium,
            0.70 * SequenceSMAvailability,
            PriceNormMedium - PriceNormLong,
            0.30 * SequenceMLAvailability,
            0.0,
            0.0),
        -1.0,
        1.0);

    const double HorizonSequence = YI_Clamp(
        0.55 * FlowSequence + 0.45 * PriceSequence,
        -1.0,
        1.0);

    const double ReboundNorm = YI_Clamp(
        Short.ReboundFromLowTicks / ExpectedShortTicks,
        0.0,
        1.0);
    const double PullbackNorm = YI_Clamp(
        Short.PullbackFromHighTicks / ExpectedShortTicks,
        0.0,
        1.0);
    const double ShortPriceTurn = YI_Clamp(
        0.70 * PriceNormShort + 0.30 * (ReboundNorm - PullbackNorm),
        -1.0,
        1.0);

    const double PriorShortAvailability = Metrics.PreviousShortReady ? 1.0 : 0.0;
    const double ImprovementAvailability = YI_Clamp(
        0.50 * SequenceSMAvailability
            + 0.25 * SequenceMLAvailability
            + 0.25 * PriorShortAvailability,
        0.0,
        1.0);

    const double FlowImprovement = YI_Clamp(
        YI_WeightedAverage3(
            FlowShort - FlowMedium,
            0.50 * SequenceSMAvailability,
            FlowMedium - FlowLong,
            0.25 * SequenceMLAvailability,
            FlowShort - PreviousFlowShort,
            0.25 * PriorShortAvailability),
        -1.0,
        1.0);

    const double CoreNumerator =
        0.40 * AbsorptionEfficiency
        + 0.25 * ShortPriceTurn
        + 0.20 * HorizonSequence * SequenceAvailability
        + 0.15 * FlowImprovement * ImprovementAvailability;
    const double CoreDenominator =
        0.40
        + 0.25
        + 0.20 * SequenceAvailability
        + 0.15 * ImprovementAvailability;
    const double CoreReversalSignal = YI_Clamp(
        CoreNumerator / YI_Max(CoreDenominator, 0.01),
        -1.0,
        1.0);

    const int FlushRecencySeconds = InputFlushRecencySeconds.GetInt();
    const double LowRecencyWeight = Long.LowAgeSeconds <= FlushRecencySeconds
        ? 1.0
        : YI_Clamp(
            2.0 - Long.LowAgeSeconds / YI_Max(FlushRecencySeconds, 1),
            0.0,
            1.0);
    const double HighRecencyWeight = Long.HighAgeSeconds <= FlushRecencySeconds
        ? 1.0
        : YI_Clamp(
            2.0 - Long.HighAgeSeconds / YI_Max(FlushRecencySeconds, 1),
            0.0,
            1.0);

    const double DropToLowTicks = (Long.ReferencePrice - Long.Low) / TickSize;
    const double RiseToHighTicks = (Long.High - Long.ReferencePrice) / TickSize;

    const double DownContext = YI_Clamp(
        YI_Max(
            -PriceNormLong,
            LowRecencyWeight * DropToLowTicks / ExpectedLongTicks),
        0.0,
        1.0);
    const double UpContext = YI_Clamp(
        YI_Max(
            PriceNormLong,
            HighRecencyWeight * RiseToHighTicks / ExpectedLongTicks),
        0.0,
        1.0);

    const double DirectionalFlowPressure = YI_Clamp(
        0.60 * FlowShort + 0.25 * FlowMedium + 0.15 * FlowLong,
        -1.0,
        1.0);
    const double DirectionalPricePressure = YI_Clamp(
        0.70 * PriceNormShort + 0.20 * PriceNormMedium + 0.10 * PriceNormLong,
        -1.0,
        1.0);
    const double ContinuationPressure = YI_Clamp(
        0.65 * DirectionalFlowPressure + 0.35 * DirectionalPricePressure,
        -1.0,
        1.0);

    const double ContextStrength = YI_Max(DownContext, UpContext);
    const double ContextScale = 0.25 + 0.75 * ContextStrength;

    const double MinimumVolumeForCoverage = InputMinimumVolume.GetInt()
        * YI_Clamp(Short.CoverageRatio, 0.10, 1.0);
    const double MinimumTradesForCoverage = InputMinimumTrades.GetInt()
        * YI_Clamp(Short.CoverageRatio, 0.10, 1.0);

    const double VolumeGate = InputMinimumVolume.GetInt() > 0
        ? YI_Clamp(Short.TotalVolume / YI_Max(MinimumVolumeForCoverage, 1.0), 0.0, 1.0)
        : 1.0;
    const double TradeGate = InputMinimumTrades.GetInt() > 0
        ? YI_Clamp(Short.Trades / YI_Max(MinimumTradesForCoverage, 1.0), 0.0, 1.0)
        : 1.0;
    const double ParticipationGate = sqrt(VolumeGate * TradeGate);

    double PaceActivityNorm = 0.0;
    if (Long.CoverageSeconds >= 30.0 && Long.Trades > 0.0)
    {
        const double ShortTradeRate = Short.Trades / YI_Max(Short.CoverageSeconds, 1.0);
        const double LongTradeRate = Long.Trades / YI_Max(Long.CoverageSeconds, 1.0);
        const double PaceRatio = ShortTradeRate / YI_Max(LongTradeRate, 0.10);
        PaceActivityNorm = YI_Clamp((PaceRatio - 0.80) / 0.80, 0.0, 1.0);
    }

    const double PaceAccelerationNorm = Metrics.PreviousShortReady
        ? YI_Clamp(Metrics.PaceAccelerationPct / 50.0, -1.0, 1.0)
        : 0.0;
    const double PaceConfidence = YI_Clamp(
        0.78
        + 0.14 * PaceActivityNorm
        + 0.08 * PaceAccelerationNorm,
        0.60,
        1.0);

    const double DeltaMagnitudeConfidence = 0.75 + 0.25 * YI_Clamp(
        YI_WeightedAverage3(
            YI_Abs(FlowShort), 0.55,
            YI_Abs(FlowMedium), 0.30,
            YI_Abs(FlowLong), 0.15),
        0.0,
        1.0);

    const double Readiness = YI_Clamp(
        0.45 * Short.CoverageRatio
        + 0.30 * Medium.CoverageRatio
        + 0.25 * Long.CoverageRatio,
        0.0,
        1.0);

    const int ReadyToScore = Short.CoverageSeconds
        >= InputMinimumStartupSeconds.GetInt()
        && Short.HasBidAskData;

    double CompositeScore = 0.0;
    if (ReadyToScore)
    {
        CompositeScore = 100.0
            * Readiness
            * ParticipationGate
            * PaceConfidence
            * DeltaMagnitudeConfidence
            * (0.82 * CoreReversalSignal * ContextScale
                + 0.18 * ContinuationPressure);
        CompositeScore = YI_Clamp(CompositeScore, -100.0, 100.0);
    }

    const double StrongThresholdValue = InputStrongThreshold.GetFloat();
    int CurrentStateCode = 0;
    const char* StateText = "Neutral";

    if (!ReadyToScore)
    {
        StateText = "Starting";
    }
    else if (!Medium.HasBidAskData || !Long.HasBidAskData)
    {
        StateText = "Missing bid/ask volume";
    }
    else if (ParticipationGate < 0.40)
    {
        StateText = "Low activity";
    }
    else if (DownContext >= 0.30)
    {
        if (CompositeScore >= StrongThresholdValue
            && Short.PriceChange > 0.0
            && FlowImprovement > 0.0)
        {
            StateText = "Bullish reversal pressure";
            CurrentStateCode = 4;
        }
        else if (CompositeScore >= AbsorptionThresholdValue
            && AbsorptionEfficiency > 0.15)
        {
            StateText = "Bullish absorption";
            CurrentStateCode = 3;
        }
        else if (FlowImprovement > 0.08
            || ShortPriceTurn > 0.08
            || CompositeScore > 10.0)
        {
            StateText = "Sell pressure weakening";
            CurrentStateCode = 2;
        }
        else if (DirectionalFlowPressure < -0.35
            && PriceNormShort < -0.10)
        {
            StateText = "Strong sell pressure";
            CurrentStateCode = -2;
        }
    }
    else if (UpContext >= 0.30)
    {
        if (CompositeScore <= -StrongThresholdValue
            && Short.PriceChange < 0.0
            && FlowImprovement < 0.0)
        {
            StateText = "Bearish reversal pressure";
            CurrentStateCode = -4;
        }
        else if (CompositeScore <= -AbsorptionThresholdValue
            && AbsorptionEfficiency < -0.15)
        {
            StateText = "Bearish absorption";
            CurrentStateCode = -3;
        }
        else if (FlowImprovement < -0.08
            || ShortPriceTurn < -0.08
            || CompositeScore < -10.0)
        {
            StateText = "Buy pressure weakening";
            CurrentStateCode = -2;
        }
        else if (DirectionalFlowPressure > 0.35
            && PriceNormShort > 0.10)
        {
            StateText = "Strong buy pressure";
            CurrentStateCode = 2;
        }
    }
    else if (ContinuationPressure <= -0.55)
    {
        StateText = "Strong sell pressure";
        CurrentStateCode = -2;
    }
    else if (ContinuationPressure >= 0.55)
    {
        StateText = "Strong buy pressure";
        CurrentStateCode = 2;
    }

    Score[sc.Index] = static_cast<float>(CompositeScore);
    VIShort[sc.Index] = static_cast<float>(Short.ImbalancePct);
    VIMedium[sc.Index] = static_cast<float>(Medium.ImbalancePct);
    VILong[sc.Index] = static_cast<float>(Long.ImbalancePct);
    PaceAcceleration[sc.Index] = static_cast<float>(Metrics.PaceAccelerationPct);
    PriceShort[sc.Index] = static_cast<float>(Short.PriceChange);
    PriceMedium[sc.Index] = static_cast<float>(Medium.PriceChange);
    PriceLong[sc.Index] = static_cast<float>(Long.PriceChange);
    DeltaShort[sc.Index] = static_cast<float>(Short.Delta);
    DeltaMedium[sc.Index] = static_cast<float>(Medium.Delta);
    DeltaLong[sc.Index] = static_cast<float>(Long.Delta);
    CoverageConfidence[sc.Index] = static_cast<float>(100.0 * Readiness);
    StateCode[sc.Index] = static_cast<float>(CurrentStateCode);

    const unsigned int PositiveColor = RGB(0, 200, 255);
    const unsigned int BullishStateColor = RGB(0, 220, 125);
    const unsigned int NegativeColor = RGB(255, 65, 65);
    const unsigned int NeutralColor = RGB(185, 190, 195);

    if (CompositeScore > 0.5)
        Score.DataColor[sc.Index] = CurrentStateCode >= 3 ? BullishStateColor : PositiveColor;
    else if (CompositeScore < -0.5)
        Score.DataColor[sc.Index] = NegativeColor;
    else
        Score.DataColor[sc.Index] = NeutralColor;

    VIShort.DataColor[sc.Index] = Short.ImbalancePct >= 0.0
        ? VIShort.PrimaryColor
        : VIShort.SecondaryColor;
    VIMedium.DataColor[sc.Index] = Medium.ImbalancePct >= 0.0
        ? VIMedium.PrimaryColor
        : VIMedium.SecondaryColor;
    VILong.DataColor[sc.Index] = Long.ImbalancePct >= 0.0
        ? VILong.PrimaryColor
        : VILong.SecondaryColor;

    if (sc.Index == sc.ArraySize - 1)
    {
        const int DrawingBase = 9100000 + (sc.StudyGraphInstanceID % 10000) * 100;

        SCString ShortLabel;
        SCString MediumLabel;
        SCString LongLabel;
        YI_FormatWindowLabelWithCoverage(ShortSeconds, Short.CoverageRatio, ShortLabel);
        YI_FormatWindowLabelWithCoverage(MediumSeconds, Medium.CoverageRatio, MediumLabel);
        YI_FormatWindowLabelWithCoverage(LongSeconds, Long.CoverageRatio, LongLabel);

        SCString DeltaShortText;
        SCString DeltaMediumText;
        SCString DeltaLongText;
        YI_FormatSignedCompactNumber(Short.Delta, DeltaShortText);
        YI_FormatSignedCompactNumber(Medium.Delta, DeltaMediumText);
        YI_FormatSignedCompactNumber(Long.Delta, DeltaLongText);

        SCString PhaseText;
        if (!ReadyToScore)
        {
            PhaseText.Format(
                "START %.0f/%ds",
                Short.CoverageSeconds,
                InputMinimumStartupSeconds.GetInt());
        }
        else if (Long.CoverageRatio < 0.95)
        {
            PhaseText.Format("EARLY %.0f%%", 100.0 * Readiness);
        }
        else
        {
            PhaseText = "LIVE";
        }

        SCString PaceText;
        if (Metrics.PreviousShortReady)
            PaceText.Format("%+.0f%%", Metrics.PaceAccelerationPct);
        else
            PaceText = "--";

        SCString Text;
        Text.Format(
            "Abs %+.0f | %s | %s\n"
            "VI %s %+.0f | %s %+.0f | %s %+.0f | Pace %s\n"
            "Px %+.2f / %+.2f / %+.2f | D %s / %s / %s",
            CompositeScore,
            PhaseText.GetChars(),
            StateText,
            ShortLabel.GetChars(),
            Short.ImbalancePct,
            MediumLabel.GetChars(),
            Medium.ImbalancePct,
            LongLabel.GetChars(),
            Long.ImbalancePct,
            PaceText.GetChars(),
            Short.PriceChange,
            Medium.PriceChange,
            Long.PriceChange,
            DeltaShortText.GetChars(),
            DeltaMediumText.GetChars(),
            DeltaLongText.GetChars());

        unsigned int TextColor = NeutralColor;
        if (CurrentStateCode >= 3)
            TextColor = BullishStateColor;
        else if (CompositeScore > 0.5)
            TextColor = PositiveColor;
        else if (CompositeScore < -0.5 || CurrentStateCode < 0)
            TextColor = NegativeColor;

        YI_DrawTextPanel(
            sc,
            DrawingBase,
            InputTextHorizontalPosition.GetInt(),
            InputTextVerticalPosition.GetFloat(),
            InputTextFontSize.GetInt(),
            TextColor,
            Text);

        if (InputShowGauges.GetYesNo())
        {
            const int GaugeCenterX = InputGaugeCenterX.GetInt();
            const int GaugeHalfWidth = InputGaugeHalfWidth.GetInt();

            YI_DrawGauge(sc, DrawingBase + 10, GaugeCenterX, GaugeHalfWidth,
                62.0f, 69.0f, Short.ImbalancePct,
                PositiveColor, NegativeColor, NeutralColor);
            YI_DrawGauge(sc, DrawingBase + 20, GaugeCenterX, GaugeHalfWidth,
                49.0f, 56.0f, Medium.ImbalancePct,
                PositiveColor, NegativeColor, NeutralColor);
            YI_DrawGauge(sc, DrawingBase + 30, GaugeCenterX, GaugeHalfWidth,
                36.0f, 43.0f, Long.ImbalancePct,
                PositiveColor, NegativeColor, NeutralColor);
            YI_DrawGauge(sc, DrawingBase + 40, GaugeCenterX, GaugeHalfWidth,
                15.0f, 24.0f, CompositeScore,
                BullishStateColor, NegativeColor, NeutralColor);
        }

        if (InputEnableAlerts.GetYesNo() && !sc.IsFullRecalculation && ReadyToScore)
        {
            const double AlertThreshold = InputAlertThreshold.GetFloat();
            int& LastAlertDirection = sc.GetPersistentInt(11);
            SCDateTime& LastAlertDateTime = sc.GetPersistentSCDateTime(11);

            SCDateTime CurrentAlertDateTime = sc.LatestDateTimeForLastBar;
            if (CurrentAlertDateTime.GetAsDouble() == 0.0)
                CurrentAlertDateTime = sc.BaseDateTimeIn[sc.Index];

            int CooldownExpired = 1;
            if (LastAlertDateTime.GetAsDouble() != 0.0)
            {
                SCDateTime NextAllowedAlert = LastAlertDateTime;
                NextAllowedAlert += SCDateTime::SECONDS(
                    InputAlertCooldownSeconds.GetInt());
                CooldownExpired = CurrentAlertDateTime >= NextAllowedAlert;
            }

            if (CompositeScore < AlertThreshold - 5.0
                && CompositeScore > -AlertThreshold + 5.0)
            {
                LastAlertDirection = 0;
            }

            if (CooldownExpired
                && CompositeScore >= AlertThreshold
                && LastAlertDirection != 1)
            {
                SCString Message;
                Message.Format("Fast flush reversal: %s | Score %+.0f", StateText, CompositeScore);
                sc.SetAlert(InputAlertNumber.GetInt(), Message);
                LastAlertDirection = 1;
                LastAlertDateTime = CurrentAlertDateTime;
            }
            else if (CooldownExpired
                && CompositeScore <= -AlertThreshold
                && LastAlertDirection != -1)
            {
                SCString Message;
                Message.Format("Fast bearish mirror: %s | Score %+.0f", StateText, CompositeScore);
                sc.SetAlert(InputAlertNumber.GetInt(), Message);
                LastAlertDirection = -1;
                LastAlertDateTime = CurrentAlertDateTime;
            }
        }
    }
}
