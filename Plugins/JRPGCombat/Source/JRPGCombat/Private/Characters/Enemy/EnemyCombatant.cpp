#include "Characters/Enemy/EnemyCombatant.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

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
