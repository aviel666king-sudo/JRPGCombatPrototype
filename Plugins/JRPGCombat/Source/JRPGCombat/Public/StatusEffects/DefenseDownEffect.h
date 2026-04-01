#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "DefenseDownEffect.generated.h"

/** Increases incoming damage by 25%. Applied by Armor Breach. */
UCLASS()
class JRPGCOMBAT_API UDefenseDownEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UDefenseDownEffect()
    {
        DisplayName   = FText::FromString("Defense Down");
        // Duration = 3 turns. Ability is authoritative — this class is NOT.
        // TODO: Future — duration will be 2-4 turns based on minigame performance.
        //       Abilities should call ApplyEffectWithDuration(OverrideDuration) where
        //       OverrideDuration comes from the minigame result (Perfect=4, Normal=3, Fail=2).
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 10;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Effect")
    float Amplifier = 0.25f;

    virtual void OnBeforeTakeDamage_Implementation(FDamagePayload& Payload) override;
};
