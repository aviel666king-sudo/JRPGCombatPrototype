#include "UI/FastTravelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Exploration/Checkpoint.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Travel/JrpgTravelSubsystem.h"
#include "Travel/VisitedCheckpointRegistry.h"

namespace
{
    FName GetCanonicalLevelName(const UWorld* World)
    {
        if (!World) { return NAME_None; }
        FString MapName = World->GetMapName();
        MapName.RemoveFromStart(World->StreamingLevelsPrefix);
        return FName(*FPaths::GetBaseFilename(MapName));
    }
}

// -----------------------------------------------------------------------------
//  UFastTravelButton
// -----------------------------------------------------------------------------

void UFastTravelButton::HandleClicked()
{
    if (OnTravelClicked)
    {
        OnTravelClicked(TargetCheckpointId);
    }
}

// -----------------------------------------------------------------------------
//  UFastTravelWidget
// -----------------------------------------------------------------------------

FSlateFontInfo UFastTravelWidget::MakeFont(int32 Size, bool bBold)
{
    FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
    return Font;
}

TSharedRef<SWidget> UFastTravelWidget::RebuildWidget()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FastTravelRoot"));
    WidgetTree->RootWidget = Root;

    // Centered dark panel.
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrushColor(FLinearColor(0.02f, 0.04f, 0.08f, 0.92f));
    Panel->SetPadding(FMargin(24.f));

    UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    PanelSlot->SetSize(FVector2D(560.f, 520.f));

    UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
    Panel->SetContent(Column);

    // Title
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
    Title->SetText(FText::FromString(TEXT("Travel to Another Checkpoint")));
    Title->SetFont(MakeFont(20, true));
    Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
    TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    // List container (Populate fills this in NativeConstruct).
    ListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ListBox"));
    UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(ListBox);
    ListSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    // Close button row.
    UFastTravelButton* CloseBtn = WidgetTree->ConstructWidget<UFastTravelButton>(UFastTravelButton::StaticClass(), TEXT("CloseBtn"));
    UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLabel"));
    CloseLabel->SetText(FText::FromString(TEXT("Close (Esc)")));
    CloseLabel->SetFont(MakeFont(14, true));
    CloseLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    CloseBtn->SetContent(CloseLabel);
    CloseBtn->TargetCheckpointId = NAME_None;
    CloseBtn->OnTravelClicked = [this](FName /*Unused*/) {
        if (OnCloseRequested) { OnCloseRequested(); }
    };
    CloseBtn->OnClicked.AddDynamic(CloseBtn, &UFastTravelButton::HandleClicked);

    UVerticalBoxSlot* CloseSlot = Column->AddChildToVerticalBox(CloseBtn);
    CloseSlot->SetHorizontalAlignment(HAlign_Right);

    return Super::RebuildWidget();
}

void UFastTravelWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Populate();
}

void UFastTravelWidget::Populate()
{
    if (!ListBox) { return; }
    ListBox->ClearChildren();

    UWorld* World = GetWorld();
    if (!World) { return; }
    UGameInstance* GI = World->GetGameInstance();
    UVisitedCheckpointRegistry* Reg = GI ? GI->GetSubsystem<UVisitedCheckpointRegistry>() : nullptr;
    if (!Reg) { return; }

    const FName LevelName = GetCanonicalLevelName(World);
    TArray<FName> Visited;
    Reg->GetVisitedInLevel(LevelName, Visited);

    // Build a lookup of live ACheckpoint actors by id for display-name + sanity.
    TMap<FName, ACheckpoint*> ByID;
    for (TActorIterator<ACheckpoint> It(World); It; ++It)
    {
        if (ACheckpoint* CP = *It) { ByID.Add(CP->GetCheckpointId(), CP); }
    }

    int32 Shown = 0;
    for (const FName& Id : Visited)
    {
        if (Id == OriginCheckpointId) { continue; }       // hide where the player already is

        ACheckpoint* Match = ByID.FindRef(Id);
        FText Label = (Match && !Match->DisplayName.IsEmpty())
            ? Match->DisplayName
            : FText::FromName(Id);

        UFastTravelButton* Btn = WidgetTree->ConstructWidget<UFastTravelButton>(UFastTravelButton::StaticClass());
        UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        BtnLabel->SetText(Label);
        BtnLabel->SetFont(MakeFont(16, false));
        BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Btn->SetContent(BtnLabel);
        Btn->TargetCheckpointId = Id;
        Btn->OnTravelClicked = [this](FName CheckpointId) { HandleTravelClicked(CheckpointId); };
        Btn->OnClicked.AddDynamic(Btn, &UFastTravelButton::HandleClicked);

        UVerticalBoxSlot* RowSlot = ListBox->AddChildToVerticalBox(Btn);
        RowSlot->SetPadding(FMargin(0.f, 4.f));
        ++Shown;
    }

    if (Shown == 0)
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Empty->SetText(FText::FromString(TEXT("No other checkpoints rested at yet.")));
        Empty->SetFont(MakeFont(14, false));
        Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.f)));
        ListBox->AddChildToVerticalBox(Empty);
    }
}

void UFastTravelWidget::HandleTravelClicked(FName CheckpointId)
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn) { return; }

    // Find the target checkpoint actor by id in the current world.
    ACheckpoint* Target = nullptr;
    for (TActorIterator<ACheckpoint> It(World); It; ++It)
    {
        if (*It && (*It)->GetCheckpointId() == CheckpointId) { Target = *It; break; }
    }
    if (!Target) { return; }

    UGameInstance* GI = World->GetGameInstance();
    UJrpgTravelSubsystem* Travel = GI ? GI->GetSubsystem<UJrpgTravelSubsystem>() : nullptr;
    if (!Travel) { return; }

    FTransform Where = Target->GetActorTransform();
    // Lift slightly so we land above the checkpoint mesh rather than inside it.
    FVector Loc = Where.GetLocation();
    Loc.Z += 50.f;
    Where.SetLocation(Loc);

    Travel->TeleportPawnTo(Pawn, Where);

    if (OnCloseRequested) { OnCloseRequested(); }
}
