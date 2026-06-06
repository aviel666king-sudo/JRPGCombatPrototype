#include "UI/MainMenuWidget.h"
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

namespace MainMenuUI {}
using namespace MainMenuUI;
namespace MainMenuUI
{
    enum { ActLoad = 0, ActNew = 1, ActDelete = 2 };

    const FLinearColor ColBG     (0.0f, 0.0f, 0.0f, 1.0f);
    const FLinearColor ColCard   (0.05f, 0.06f, 0.09f, 1.0f);
    const FLinearColor ColText   (0.92f, 0.94f, 0.98f, 1.0f);
    const FLinearColor ColSubtle (0.55f, 0.58f, 0.64f, 1.0f);
    const FLinearColor ColPlay   (0.15f, 0.32f, 0.22f, 1.0f);
    const FLinearColor ColDelete (0.30f, 0.12f, 0.12f, 1.0f);

    FSlateFontInfo Font(int32 Size, bool bBold = false)
    {
        return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
    }
}

USaveSubsystem* UMainMenuWidget::GetSaveSys() const
{
    UWorld* W = GetWorld();
    UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
    return GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
}

TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
    Screen->SetBrushColor(ColBG);
    Screen->SetHorizontalAlignment(HAlign_Center);
    Screen->SetVerticalAlignment(VAlign_Center);
    UCanvasPanelSlot* SS = Root->AddChildToCanvas(Screen);
    SS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    SS->SetOffsets(FMargin(0.f));

    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Screen->SetContent(Box);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Title->SetText(FText::FromString(TEXT("JRPG  PROTOTYPE")));
    Title->SetFont(Font(40, true));
    Title->SetColorAndOpacity(FSlateColor(ColText));
    Title->SetJustification(ETextJustify::Center);
    UVerticalBoxSlot* TS = Box->AddChildToVerticalBox(Title);
    TS->SetHorizontalAlignment(HAlign_Center);
    TS->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    UTextBlock* Sub = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Sub->SetText(FText::FromString(TEXT("Choose a save slot")));
    Sub->SetFont(Font(15, false));
    Sub->SetColorAndOpacity(FSlateColor(ColSubtle));
    Sub->SetJustification(ETextJustify::Center);
    UVerticalBoxSlot* SuS = Box->AddChildToVerticalBox(Sub);
    SuS->SetHorizontalAlignment(HAlign_Center);
    SuS->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));

    SlotList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Box->AddChildToVerticalBox(SlotList);

    return Super::RebuildWidget();
}

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RebuildSlots();
}

void UMainMenuWidget::RebuildSlots()
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
        L->SetFont(Font(15, true));
        L->SetColorAndOpacity(FSlateColor(ColText));
        Btn->SetContent(L);
        return Btn;
    };

    for (int32 i = 0; i < 3; ++i)
    {
        UJrpgSaveGame* Save = Sys ? Sys->PeekSlot(SlotName(i)) : nullptr;

        UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Card->SetBrushColor(ColCard);
        Card->SetPadding(FMargin(16.f, 12.f));

        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Card->SetContent(Row);

        UTextBlock* Info = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Info->SetText(FText::FromString(Save
            ? FString::Printf(TEXT("Slot %d   -   %s"), i + 1, *Save->DisplayName)
            : FString::Printf(TEXT("Slot %d   -   Empty"), i + 1)));
        Info->SetFont(Font(16, false));
        Info->SetColorAndOpacity(FSlateColor(Save ? ColText : ColSubtle));
        UHorizontalBoxSlot* IS = Row->AddChildToHorizontalBox(Info);
        IS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        IS->SetVerticalAlignment(VAlign_Center);

        if (Save)
        {
            UHorizontalBoxSlot* B1 = Row->AddChildToHorizontalBox(
                MakeButton(TEXT("Continue"), ColPlay, i, ActLoad));
            B1->SetPadding(FMargin(4.f, 0.f));
            UHorizontalBoxSlot* B2 = Row->AddChildToHorizontalBox(
                MakeButton(TEXT("Delete"), ColDelete, i, ActDelete));
            B2->SetPadding(FMargin(4.f, 0.f));
        }
        else
        {
            UHorizontalBoxSlot* B1 = Row->AddChildToHorizontalBox(
                MakeButton(TEXT("New Game"), ColPlay, i, ActNew));
            B1->SetPadding(FMargin(4.f, 0.f));
        }

        UVerticalBoxSlot* CS = SlotList->AddChildToVerticalBox(Card);
        CS->SetPadding(FMargin(0.f, 6.f));
    }
}

void UMainMenuWidget::HandleAction(UMenuSlotButton* Btn)
{
    USaveSubsystem* Sys = GetSaveSys();
    if (!Btn || !Sys) { return; }

    const FString SlotStr = SlotName(Btn->SlotIndex);
    switch (Btn->Action)
    {
        case ActLoad:   Sys->LoadFromSlot(SlotStr);   break;   // travels to saved level
        case ActNew:    Sys->StartNewGame(SlotStr);   break;   // fresh start
        case ActDelete: Sys->DeleteSlot(SlotStr); RebuildSlots(); break;
    }
}
