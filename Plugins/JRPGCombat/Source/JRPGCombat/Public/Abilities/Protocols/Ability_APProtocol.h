#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_APProtocol.generated.h"

/**
 * AP Protocol
 * Grants +5 AP to a living ally (including the acting character themselves).
 * Target scope: SingleAlly.
 * Ends the turn.  Charges deducted by BattleManager before Execute is called.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_APProtocol : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_APProtocol()
    {
        DisplayName     = FText::FromString(TEXT("AP Protocol"));
        bEndsTurn       = true;
        AbilityCategory = EAbilityCategory::None;
        TargetScope     = ETargetScope::SingleAlly;
    }
protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
