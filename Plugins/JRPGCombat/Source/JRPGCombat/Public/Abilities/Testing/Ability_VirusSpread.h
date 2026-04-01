#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_VirusSpread.generated.h"

/**
 * Applies 4 stacks of Virus Spread to the target.
 * Each stack = 1 tick at turn start (6% MaxHP Magical + spread/retention rolls).
 * The ability decides stack count — the effect itself is stack-agnostic.
 * Cost: 3 AP. Ends the turn.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_VirusSpread : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_VirusSpread()
    {
        DisplayName = FText::FromString("Virus Spread");
        bEndsTurn   = true;
        Costs.Add({ EResourceType::AP, 3.f });
    }

    /** Number of stacks applied by this ability. Change here without touching the effect. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    int32 StacksToApply = 4;

protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
