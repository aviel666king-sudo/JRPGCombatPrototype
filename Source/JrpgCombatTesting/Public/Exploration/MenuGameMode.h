#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MenuGameMode.generated.h"

/**
 * AMenuGameMode
 *
 * GameMode for the L_MainMenu start map. On BeginPlay it shows the main-menu
 * slot picker over a black screen with UI input. No gameplay pawn needed.
 */
UCLASS()
class JRPGCOMBATTESTING_API AMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMenuGameMode();

    /** Optional designer override; falls back to the C++ UMainMenuWidget. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Menu")
    TSubclassOf<class UUserWidget> MenuWidgetClass;

protected:
    virtual void BeginPlay() override;

    UPROPERTY()
    TObjectPtr<class UUserWidget> MenuWidget;
};
