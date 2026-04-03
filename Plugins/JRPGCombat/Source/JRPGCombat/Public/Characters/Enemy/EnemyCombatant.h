#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CombatantBase.h"
#include "EnemyCombatant.generated.h"

/**
 * AEnemyCombatant
 *
 * Structural base for all enemy combatants.
 * Sets Team = Enemy and provides a future extension point for
 * AI-related hooks (decision making, threat evaluation, etc.).
 *
 * AP is not removed from the system but enemies do not rely on it
 * as a gameplay constraint. Enemy abilities should be authored with
 * Cost = 0 so the AP pool never blocks AI actions.
 *
 *   ACombatantBase
 *     → AEnemyCombatant
 *         → (specific enemy types, if needed)
 */
UCLASS(BlueprintType, Blueprintable)
class JRPGCOMBAT_API AEnemyCombatant : public ACombatantBase
{
    GENERATED_BODY()

public:

    AEnemyCombatant();
};
