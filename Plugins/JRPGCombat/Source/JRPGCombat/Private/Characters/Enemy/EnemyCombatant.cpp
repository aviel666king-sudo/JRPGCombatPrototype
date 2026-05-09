#include "Characters/Enemy/EnemyCombatant.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Core/DangerManager.h"
#include "Engine/GameInstance.h"

AEnemyCombatant::AEnemyCombatant()
{
    // ── Enemy mesh: Quinn ─────────────────────────────────────────────────────
    // Set in C++ so the assignment survives C++ recompiles without needing
    // manual Blueprint reassignment after every hot-reload.
    {
        static ConstructorHelpers::FObjectFinder<USkeletalMesh> QuinnSK(
            TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
        if (QuinnSK.Succeeded() && Mesh) { Mesh->SetSkeletalMeshAsset(QuinnSK.Object); }
    }
}

void AEnemyCombatant::InitializeForBattle()
{
    if (!bIsBoss)
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UDangerManager* Danger = GI->GetSubsystem<UDangerManager>())
            {
                const float Mult = Danger->GetStatMultiplier();
                if (!FMath::IsNearlyEqual(Mult, 1.f))
                {
                    BaseStats.Attack  *= Mult;
                    BaseStats.Defense *= Mult;
                    BaseStats.MaxHP   *= Mult;
                    UE_LOG(LogTemp, Log,
                        TEXT("[Danger] %s scaled by x%.2f (HP=%.0f Atk=%.0f Def=%.0f)"),
                        *GetName(), Mult,
                        BaseStats.MaxHP, BaseStats.Attack, BaseStats.Defense);
                }
            }
        }
    }

    Super::InitializeForBattle();
}
