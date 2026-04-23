// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BatchProcEditor/CDGBatchProgressWindow.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"

#define LOCTEXT_NAMESPACE "CDGBatchProgressWindow"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

/** "x / N" label for one loop dimension.  Shows "--/--" before the batch starts. */
static FText DimText(int32 Cur, int32 Tot)
{
	if (Tot <= 0) return FText::FromString(TEXT("–/–"));
	return FText::Format(FText::FromString(TEXT("{0}/{1}")),
		FText::AsNumber(Cur), FText::AsNumber(Tot));
}

// ─────────────────────────────────────────────────────────────────────────────
// Construct
// ─────────────────────────────────────────────────────────────────────────────

void SBatchProgressWindow::Construct(const FArguments& InArgs)
{
	OnCancelRequested = InArgs._OnCancelRequested;

	// Shared small-font style
	const FSlateFontInfo SmallFont     = FAppStyle::GetFontStyle("SmallFont");
	const FSlateFontInfo SmallBoldFont = FAppStyle::GetFontStyle("SmallFontBold");
	const FSlateFontInfo NormalFont    = FAppStyle::GetFontStyle("NormalFont");

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(14.f, 10.f))
		[
			SNew(SVerticalBox)

			// ── Row 1: [Level x/N] – [Anchor x/N] – [Char x/N] – [Anim x/N]  [Cancel] ──
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 5.f)
			[
				SNew(SHorizontalBox)

				// Level
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LevelLbl", "Level"))
					.Font(SmallFont)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return DimText(Detail.LevelCurrent, Detail.LevelTotal); })
					.Font(SmallBoldFont)
				]

				// Anchor
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnchorLbl", "Anchor"))
					.Font(SmallFont)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return DimText(Detail.AnchorCurrent, Detail.AnchorTotal); })
					.Font(SmallBoldFont)
				]

				// Character
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CharLbl", "Char"))
					.Font(SmallFont)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return DimText(Detail.CharacterCurrent, Detail.CharacterTotal); })
					.Font(SmallBoldFont)
				]

				// Anim
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnimLbl", "Anim"))
					.Font(SmallFont)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return DimText(Detail.AnimCurrent, Detail.AnimTotal); })
					.Font(SmallBoldFont)
				]

				// Shot
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShotLbl", "Shot"))
					.Font(SmallFont)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return DimText(Detail.ShotCurrent, Detail.ShotTotal); })
					.Font(SmallBoldFont)
				]

				// Spacer
				+ SHorizontalBox::Slot().FillWidth(1.f)

				// Cancel / Close button
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "DangerButton")
					.Text_Lambda([this]()
					{
						return bDone ? LOCTEXT("CloseBtn", "Close") : LOCTEXT("CancelBtn", "Cancel");
					})
					.ToolTipText_Lambda([this]()
					{
						return bDone
							? LOCTEXT("CloseTip", "Close this window.")
							: LOCTEXT("CancelTip", "Stop the batch after the current render and clean up.");
					})
					.OnClicked_Lambda([this]()
					{
						if (!bDone) OnCancelRequested.ExecuteIfBound();
						CloseWindow();
						return FReply::Handled();
					})
				]
			]

			// ── Row 2: progress bar + "combo x/N – pct" on right ────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(SHorizontalBox)

				// Bar (fills available width)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox).HeightOverride(8.f)
					[
				SNew(SProgressBar)
					.Percent_Lambda([this]() -> TOptional<float>
					{
						if (Detail.GlobalShotsTotal <= 0)
							return TOptional<float>(); // indeterminate until first shots are known
						return FMath::Clamp(
							static_cast<float>(Detail.GlobalShotsRendered) / Detail.GlobalShotsTotal,
							0.f, 1.f);
					})
					.FillColorAndOpacity(FSlateColor(FLinearColor(0.1f, 0.6f, 1.f)))
					]
				]

				// "combo x/N – pct%" on the right
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return MakeShotPercentText(); })
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]

			// ── Row 3: last log line ─────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return Log.IsEmpty()
						? FText::GetEmpty()
						: FText::FromString(Log.Last());
				})
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]
		]
	];
}

// ─────────────────────────────────────────────────────────────────────────────
// Text helpers
// ─────────────────────────────────────────────────────────────────────────────

FText SBatchProgressWindow::MakeShotPercentText() const
{
	if (Detail.GlobalShotsTotal <= 0)
	{
		// Fall back to combo progress before any shots are known
		if (Detail.ComboTotal <= 0) return FText::GetEmpty();
		const int32 Pct = FMath::RoundToInt(100.f * Detail.ComboCurrent / Detail.ComboTotal);
		return FText::Format(
			LOCTEXT("ComboFallback", "combo {0}/{1}  —  {2}%"),
			FText::AsNumber(Detail.ComboCurrent),
			FText::AsNumber(Detail.ComboTotal),
			FText::AsNumber(Pct));
	}

	const int32 Pct = FMath::RoundToInt(
		100.f * Detail.GlobalShotsRendered / Detail.GlobalShotsTotal);

	return FText::Format(
		LOCTEXT("ShotAndPct", "{0}/{1} shots  —  {2}%"),
		FText::AsNumber(Detail.GlobalShotsRendered),
		FText::AsNumber(Detail.GlobalShotsTotal),
		FText::AsNumber(Pct));
}

// ─────────────────────────────────────────────────────────────────────────────
// State update
// ─────────────────────────────────────────────────────────────────────────────

void SBatchProgressWindow::UpdateDetailedProgress(const FBatchDetailedProgress& InProgress)
{
	Detail = InProgress;
}

void SBatchProgressWindow::UpdateProgress(int32 Completed, int32 Total)
{
	Detail.ComboCurrent = Completed;
	Detail.ComboTotal   = Total;
}

void SBatchProgressWindow::AddLog(const FString& Msg)
{
	Log.Add(Msg);
	if (Log.Num() > 200) Log.RemoveAt(0, Log.Num() - 200);
}

void SBatchProgressWindow::MarkCompleted(bool bSuccess)
{
	bDone = true;
	Detail.ComboCurrent = Detail.ComboTotal;
	if (Detail.ShotTotal > 0)         Detail.ShotCurrent      = Detail.ShotTotal;
	if (Detail.GlobalShotsTotal > 0)  Detail.GlobalShotsRendered = Detail.GlobalShotsTotal;
	AddLog(bSuccess
		? TEXT("Batch processing finished successfully.")
		: TEXT("Batch processing was cancelled."));
}

// ─────────────────────────────────────────────────────────────────────────────
// Window management
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<SWindow> SBatchProgressWindow::OpenWindow(TSharedPtr<SWindow> ParentWindow)
{
	if (TSharedPtr<SWindow> Existing = OwningSWindow.Pin())
	{
		Existing->BringToFront();
		return Existing;
	}

	TSharedRef<SWindow> NewWindow = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Batch Processing Progress"))
		.ClientSize(FVector2D(620.f, 110.f))
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.HasCloseButton(false)
		.IsTopmostWindow(true);

	NewWindow->SetContent(SharedThis(this));

	NewWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda(
		[this](const TSharedRef<SWindow>&)
		{
			if (!bDone) OnCancelRequested.ExecuteIfBound();
		}));

	OwningSWindow = NewWindow;

	if (ParentWindow.IsValid())
		FSlateApplication::Get().AddWindowAsNativeChild(NewWindow, ParentWindow.ToSharedRef());
	else
		FSlateApplication::Get().AddWindow(NewWindow);

	return NewWindow;
}

void SBatchProgressWindow::CloseWindow()
{
	if (TSharedPtr<SWindow> Win = OwningSWindow.Pin())
	{
		Win->SetOnWindowClosed(FOnWindowClosed());
		FSlateApplication::Get().RequestDestroyWindow(Win.ToSharedRef());
		OwningSWindow.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
