#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UVerticalBox;
class USaveSubsystem;
class UMenuSlotButton;

/**
 * UMainMenuWidget
 *
 * Black start screen with 3 save slots. Each slot shows its metadata (or
 * "Empty"); a filled slot offers Continue + Delete, an empty slot offers
 * New Game. Pure C++ UMG. Shown by AMenuGameMode on the L_MainMenu map.
 */
UCLASS()
class JRPGCOMBATTESTING_API UMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

private:
    TObjectPtr<UVerticalBox> SlotList;

    USaveSubsystem* GetSaveSys() const;
    void RebuildSlots();
    void HandleAction(UMenuSlotButton* Btn);

    static FString SlotName(int32 Index) { return FString::Printf(TEXT("Save_%d"), Index + 1); }
};
