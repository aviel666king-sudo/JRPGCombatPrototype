#include "Characters/Player/PlayerCombatant.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"

APlayerCombatant::APlayerCombatant()
{
    // ── Player mesh: Manny ────────────────────────────────────────────────────
    // Set in C++ so the assignment survives C++ recompiles without needing
    // manual Blueprint reassignment after every hot-reload.
    {
        static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannySK(
            TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
        if (MannySK.Succeeded() && Mesh) { Mesh->SetSkeletalMesh(MannySK.Object); }
    }

    // ── Player-exclusive montages ─────────────────────────────────────────────
    {
        static ConstructorHelpers::FObjectFinder<UAnimMontage> Gun(
            TEXT("/Game/Characters/Mannequins/Anims/Pistol/AM_Gun.AM_Gun"));
        if (Gun.Succeeded()) { GunMontage = Gun.Object; }
    }
    {
        static ConstructorHelpers::FObjectFinder<UAnimMontage> Parry(
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/AM_Parry.AM_Parry"));
        if (Parry.Succeeded()) { ParryMontage = Parry.Object; }
    }
}
