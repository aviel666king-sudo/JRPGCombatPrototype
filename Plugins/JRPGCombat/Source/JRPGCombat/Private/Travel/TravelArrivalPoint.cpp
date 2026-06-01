#include "Travel/TravelArrivalPoint.h"

#include "Components/SceneComponent.h"

#if WITH_EDITORONLY_DATA
#include "Components/BillboardComponent.h"
#include "Components/ArrowComponent.h"
#endif

ATravelArrivalPoint::ATravelArrivalPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

#if WITH_EDITORONLY_DATA
    Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
    if (Billboard)
    {
        Billboard->SetupAttachment(RootComponent);
        Billboard->SetHiddenInGame(true);
    }

    Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrivalArrow"));
    if (Arrow)
    {
        Arrow->SetupAttachment(RootComponent);
        Arrow->SetHiddenInGame(true);
        Arrow->ArrowColor = FColor::Cyan;
    }
#endif
}
