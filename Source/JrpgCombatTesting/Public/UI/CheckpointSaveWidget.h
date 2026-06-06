#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CheckpointSaveWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class USaveSubsystem;
class URosterSubsystem;
class UMenuSlotButton;

/**
 * UCheckpointSaveWidget
 *
 * Opened at a checkpoint. Lets the player Save to one of 3 slots (or delete a
 * slot), and reset the party to base via two Clear buttons (full wipe / keep
 * loot + money). Pure C++ UMG.
 */
UCLASS()
class JRPGCOMBATTESTING_API UCheckpointSaveWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    TFunction<void()> OnCloseRequested;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual bool NativeSupportsKeyboardFocus() const override { return true; }
    virtual FReply NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Key) override;

private:
    TObjectPtr<UVerticalBox> SlotList;
    TObjectPtr<UTextBlock>   StatusText;

    USaveSubsystem*   GetSaveSys() const;
    URosterSubsystem* GetRoster() const;

    void RebuildSlots();
    void HandleAction(UMenuSlotButton* Btn);
    void SetStatus(const FString& Msg);

    static FString SlotName(int32 Index) { return FString::Printf(TEXT("Save_%d"), Index + 1); }
};
