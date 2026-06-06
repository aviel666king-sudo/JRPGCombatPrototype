#include "Exploration/MenuGameMode.h"
#include "UI/MainMenuWidget.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AMenuGameMode::AMenuGameMode()
{
    // No gameplay pawn on the menu — the slot picker is pure UI.
    DefaultPawnClass = nullptr;
}

void AMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) { return; }

    UClass* WidgetClass = MenuWidgetClass ? MenuWidgetClass.Get() : UMainMenuWidget::StaticClass();
    MenuWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
    if (!MenuWidget) { return; }

    MenuWidget->AddToViewport(0);

    PC->bShowMouseCursor = true;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(MenuWidget->TakeWidget());
    PC->SetInputMode(Mode);
}
