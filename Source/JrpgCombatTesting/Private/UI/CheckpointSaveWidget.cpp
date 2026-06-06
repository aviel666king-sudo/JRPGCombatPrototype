#include "UI/CheckpointSaveWidget.h"
#include "UI/MenuSlotButton.h"

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
#include "Styling/CoreStyle.h"

#include "Persistence/SaveSubsystem.h"
#include "Persistence/JrpgSaveGame.h"
#include "Roster/RosterSubsystem.h"

namespace CheckpointSaveUI {}
using namespace CheckpointSaveUI;
namespace CheckpointSaveUI
{
    enum { ActSave = 0, ActDelete = 1, ActClearFull = 2, ActClearKeep = 3, ActClose = 4 };

    const FLinearColor ColBG     (0.015f, 0.025f, 0.045f, 0.97f);
    const FLinearColor ColCard   (0.05f, 0.08f, 0.13f, 1.0f);
    const FLinearColor ColText   (0.92f, 0.94f, 0.98f, 1.0f);
    const FLinearColor ColSubtle (0.58f, 0.62f, 0.68f, 1.0f);
    const FLinearColor ColSave   (0.15f, 0.32f, 0.22f, 1.0f);
    const FLinearColor ColDelete (0.30f, 0.12f, 0.12f, 1.0f);
    const FLinearColor ColDanger (0.42f, 0.10f, 0.10f, 1.0f);
    const FLinearColor ColWarn   (0.40f, 0.30f, 0.10f, 1.0f);
    const FLinearColor ColBtn    (0.12f, 0.18f, 0.28f, 1.0f);

    FSlateFontInfo Font(int32 Size, bool bBold = false)
    {
        return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
    }
}

USaveSubsystem* UCheckpointSaveWidget::GetSaveSys() const
{
    UWorld* W = GetWorld();
    UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
    return GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
}

URosterSubsystem* UCheckpointSaveWidget::GetRoster() const
{
    UWorld* W = GetWorld();
    UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
    return GI ? GI->GetSubsystem<URosterSubsystem>() : nullptr;
}

TSharedRef<SWidget> UCheckpointSaveWidget::RebuildWidget()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
    Screen->SetBrushColor(ColBG);
    Screen->SetPadding(FMargin(40.f, 28.f));
    UCanvasPanelSlot* SS = Root->AddChildToCanvas(Screen);
    SS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    SS->SetOffsets(FMargin(0.f));

    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Screen->SetContent(Box);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Title->SetText(FText::FromString(TEXT("Save / Manage")));
    Title->SetFont(Font(26, true));
    Title->SetColorAndOpacity(FSlateColor(ColText));
    UVerticalBoxSlot* TS = Box->AddChildToVerticalBox(Title);
    TS->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

    SlotList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Box->AddChildToVerticalBox(SlotList);

    StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    StatusText->SetFont(Font(14, false));
    StatusText->SetColorAndOpacity(FSlateColor(ColSubtle));
    UVerticalBoxSlot* StS = Box->AddChildToVerticalBox(StatusText);
    StS->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

    return Super::RebuildWidget();
}

void UCheckpointSaveWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RebuildSlots();
}

FReply UCheckpointSaveWidget::NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Key)
{
    const FKey K = Key.GetKey();
    if (K == EKeys::Escape || K == EKeys::Tab)
    {
        if (OnCloseRequested) { OnCloseRequested(); }
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geo, Key);
}

void UCheckpointSaveWidget::RebuildSlots()
{
    if (!SlotList) { return; }
    SlotList->ClearChildren();

    USaveSubsystem* Sys = GetSaveSys();

    auto MakeButton = [&](const FString& Label, const FLinearColor& BG, int32 InSlot, int32 Action) -> UMenuSlotButton*
    {
        UMenuSlotButton* Btn = WidgetTree->ConstructWidget<UMenuSlotButton>(UMenuSlotButton::StaticClass());
        Btn->SetBackgroundColor(BG);
        Btn->SlotIndex = InSlot;
        Btn->Action    = Action;
        Btn->OnAction  = [this](UMenuSlotButton* B) { HandleAction(B); };
        Btn->OnClicked.AddDynamic(Btn, &UMenuSlotButton::HandleClicked);
        UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        L->SetText(FText::FromString(Label));
        L->SetFont(Font(14, true));
        L->SetColorAndOpacity(FSlateColor(ColText));
        Btn->SetContent(L);
        return Btn;
    };

    // ---- 3 save slots ----
    for (int32 i = 0; i < 3; ++i)
    {
        UJrpgSaveGame* Save = Sys ? Sys->PeekSlot(SlotName(i)) : nullptr;

        UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Card->SetBrushColor(ColCard);
        Card->SetPadding(FMargin(14.f, 10.f));
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Card->SetContent(Row);

        const bool bActive = Sys && Sys->ActiveSlot == SlotName(i);
        UTextBlock* Info = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Info->SetText(FText::FromString(Save
            ? FString::Printf(TEXT("Slot %d   -   %s%s"), i + 1, *Save->DisplayName, bActive ? TEXT("   (active)") : TEXT(""))
            : FString::Printf(TEXT("Slot %d   -   Empty%s"), i + 1, bActive ? TEXT("   (active)") : TEXT(""))));
        Info->SetFont(Font(15, false));
        Info->SetColorAndOpacity(FSlateColor(Save ? ColText : ColSubtle));
        UHorizontalBoxSlot* IS = Row->AddChildToHorizontalBox(Info);
        IS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        IS->SetVerticalAlignment(VAlign_Center);

        UHorizontalBoxSlot* B1 = Row->AddChildToHorizontalBox(MakeButton(TEXT("Save here"), ColSave, i, ActSave));
        B1->SetPadding(FMargin(4.f, 0.f));
        if (Save)
        {
            UHorizontalBoxSlot* B2 = Row->AddChildToHorizontalBox(MakeButton(TEXT("Delete"), ColDelete, i, ActDelete));
            B2->SetPadding(FMargin(4.f, 0.f));
        }

        UVerticalBoxSlot* CS = SlotList->AddChildToVerticalBox(Card);
        CS->SetPadding(FMargin(0.f, 4.f));
    }

    // ---- Clear buttons ----
    UTextBlock* ClrHdr = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    ClrHdr->SetText(FText::FromString(TEXT("Reset character to base")));
    ClrHdr->SetFont(Font(16, true));
    ClrHdr->SetColorAndOpacity(FSlateColor(ColSubtle));
    UVerticalBoxSlot* CH = SlotList->AddChildToVerticalBox(ClrHdr);
    CH->SetPadding(FMargin(0.f, 16.f, 0.f, 4.f));

    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        UHorizontalBoxSlot* C1 = Row->AddChildToHorizontalBox(
            MakeButton(TEXT("Full wipe (also clears loot & money)"), ColDanger, -1, ActClearFull));
        C1->SetPadding(FMargin(4.f, 0.f));
        UHorizontalBoxSlot* C2 = Row->AddChildToHorizontalBox(
            MakeButton(TEXT("Reset but keep loot & money"), ColWarn, -1, ActClearKeep));
        C2->SetPadding(FMargin(4.f, 0.f));
        SlotList->AddChildToVerticalBox(Row);
    }

    // ---- Close ----
    UMenuSlotButton* Close = MakeButton(TEXT("Close  (Esc)"), ColBtn, -1, ActClose);
    UVerticalBoxSlot* ClS = SlotList->AddChildToVerticalBox(Close);
    ClS->SetHorizontalAlignment(HAlign_Left);
    ClS->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
}

void UCheckpointSaveWidget::SetStatus(const FString& Msg)
{
    if (StatusText) { StatusText->SetText(FText::FromString(Msg)); }
}

void UCheckpointSaveWidget::HandleAction(UMenuSlotButton* Btn)
{
    if (!Btn) { return; }
    USaveSubsystem* Sys = GetSaveSys();

    switch (Btn->Action)
    {
        case ActSave:
            if (Sys)
            {
                const FString SlotStr = SlotName(Btn->SlotIndex);
                if (Sys->SaveToSlot(SlotStr))
                {
                    Sys->ActiveSlot = SlotStr;   // play continues in the slot you saved into
                    SetStatus(FString::Printf(TEXT("Saved to slot %d."), Btn->SlotIndex + 1));
                }
                else { SetStatus(TEXT("Save failed.")); }
                RebuildSlots();
            }
            break;

        case ActDelete:
            if (Sys) { Sys->DeleteSlot(SlotName(Btn->SlotIndex)); SetStatus(TEXT("Slot deleted.")); RebuildSlots(); }
            break;

        case ActClearFull:
            if (URosterSubsystem* R = GetRoster())
            {
                R->ClearCharactersToBase(/*bKeepLootAndMoney=*/false);
                SetStatus(TEXT("Character reset to base. Loot & money wiped."));
            }
            break;

        case ActClearKeep:
            if (URosterSubsystem* R = GetRoster())
            {
                R->ClearCharactersToBase(/*bKeepLootAndMoney=*/true);
                SetStatus(TEXT("Character reset to base. Loot & money kept."));
            }
            break;

        case ActClose:
            if (OnCloseRequested) { OnCloseRequested(); }
            break;
    }
}
