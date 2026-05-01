#include "Core/BattleArena.h"
#include "Components/BillboardComponent.h"

ABattleArena::ABattleArena()
{
    PrimaryActorTick.bCanEverTick = false;

    Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
    RootComponent = Billboard;
}
