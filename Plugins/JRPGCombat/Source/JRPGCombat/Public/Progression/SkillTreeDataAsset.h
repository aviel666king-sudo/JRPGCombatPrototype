#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SkillTreeDataAsset.generated.h"

class UCombatAbility;

/**
 * FSkillNode
 *
 * One unlockable node in a character's skill tree. A node grants a single
 * combat ability when bought. Buying requires every prerequisite node to be
 * unlocked first (branching tree) and enough SkillCoins.
 *
 * GridPos drives the UI layout only: X = column (tier / depth), Y = row.
 */
USTRUCT(BlueprintType)
struct JRPGCOMBAT_API FSkillNode
{
    GENERATED_BODY()

    /** Stable identifier used for unlock tracking and prerequisite links. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Node")
    FName NodeId;

    /** Player-facing name. Falls back to the ability's DisplayName if empty. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Node")
    FText DisplayName;

    /** Short tooltip describing what the skill does. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Node",
              meta = (MultiLine = "true"))
    FText Description;

    /** Ability granted when this node is unlocked. Added to the combat menu. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Node")
    TSubclassOf<UCombatAbility> AbilityClass;

    /** SkillCoins required to buy this node. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Node",
              meta = (ClampMin = "0"))
    int32 SkillCoinCost = 1;

    /** NodeIds that must be unlocked before this node can be bought. Empty =
     *  a root node, available from the start (subject only to cost). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Node")
    TArray<FName> Prerequisites;

    /** Layout-only grid position. X = column (depth), Y = row. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Node")
    FIntPoint GridPos = FIntPoint::ZeroValue;
};

/**
 * USkillTreeDataAsset
 *
 * Per-character skill tree definition. Assign one to a character (e.g. on
 * ACombatantFencer's BP) via APlayerCombatant::SkillTree. Authored in the
 * editor as a Data Asset; the unlock state itself lives on the runtime
 * APlayerCombatant (UnlockedNodes), not here.
 */
UCLASS(BlueprintType)
class JRPGCOMBAT_API USkillTreeDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Tree")
    TArray<FSkillNode> Nodes;

    /** Find a node by id. Returns nullptr if not present. */
    const FSkillNode* FindNode(FName NodeId) const
    {
        return Nodes.FindByPredicate([NodeId](const FSkillNode& N) { return N.NodeId == NodeId; });
    }
};
