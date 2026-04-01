#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_RevivalProtocol.generated.h"

/**
 * Revival Protocol
 * Revives one dead party member at 35% of their MaxHP.
 * Target scope: DeadAlly — only dead player-side combatants are valid targets.
 * Ends the turn.  Charges deducted by BattleManager before Execute is called.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_RevivalProtocol : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_RevivalProtocol()
    {
        DisplayName     = FText::FromString(TEXT("Revival Protocol"));
        bEndsTurn       = true;
        AbilityCategory = EAbilityCategory::None;
        TargetScope     = ETargetScope::DeadAlly;
    }
protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
