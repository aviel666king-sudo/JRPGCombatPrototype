#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnitStatusWidget.generated.h"

class ACombatantBase;
class UProgressBar;
class UTextBlock;
class UBorder;

/**
 * UUnitStatusWidget
 *
 * Single combatant status card — used for both player and enemy slots.
 *
 * Visual states
 * ─────────────────────────────────────────────────────────────
 *  Normal              full opacity, no tint, no border
 *  Active player       CardOverlay tinted YELLOW (semi-transparent)
 *  Targeted enemy      OuterBorder brush colour = GREEN
 *  Enemy acting        OuterBorder brush colour = ORANGE  ← NEW
 *  Dead / empty slot   entire card faded to DeadOpacity (0.35)
 *
 * Priority: enemy-acting orange overrides the green target border if the same
 * card happens to be both targeted and currently acting.  The HUD is responsible
 * for not highlighting a dead/empty card.
 *
 * UMG SETUP — same hierarchy as before.  No new named widgets needed.
 * OuterBorder now serves three states (transparent / green / orange) via
 * SetBrushColor(); only one can be active at a time.
 */
UCLASS(BlueprintType, Blueprintable)
class JRPGCOMBAT_API UUnitStatusWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------------------------
    //  UMG bound widgets (all BindWidgetOptional — missing = silently ignored)
    // -------------------------------------------------------------------------

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> OuterBorder;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> CardOverlay;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> NameText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> HPBar;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> HPText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> APBar;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> APText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PassiveText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StatusEffectsText;

    // -------------------------------------------------------------------------
    //  Configurable colours
    // -------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Status|Colors")
    FLinearColor ActivePlayerTint  = FLinearColor(1.f,  0.85f, 0.f,  0.45f);  // yellow

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Status|Colors")
    FLinearColor TargetBorderColor = FLinearColor(0.f,  0.85f, 0.2f, 1.f);    // green

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Status|Colors")
    FLinearColor EnemyActingBorderColor = FLinearColor(1.f, 0.45f, 0.f, 1.f); // orange

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Status|Colors",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DeadOpacity = 0.35f;

    // -------------------------------------------------------------------------
    //  API
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Unit Status")
    void SetUnit(ACombatantBase* InUnit);

    UFUNCTION(BlueprintCallable, Category = "Unit Status")
    void Refresh();

    /** Yellow card tint — active player cursor is on this unit. */
    UFUNCTION(BlueprintCallable, Category = "Unit Status")
    void SetIsActivePlayer(bool bActive);

    /** Green border — this enemy is the current player attack target. */
    UFUNCTION(BlueprintCallable, Category = "Unit Status")
    void SetIsTargeted(bool bTargeted);

    /**
     * Orange border — this enemy is the currently ACTING unit.
     * Overrides the green target border if both are set at once.
     * Called by CombatHUDWidget in response to OnEnemyActingChanged.
     */
    UFUNCTION(BlueprintCallable, Category = "Unit Status")
    void SetIsEnemyActing(bool bActing);

    UFUNCTION(BlueprintCallable, Category = "Unit Status")
    void SetIsFaded(bool bFaded);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Status")
    ACombatantBase* GetUnit() const { return Unit.Get(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Status")
    bool IsUnavailable() const;

protected:

    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintNativeEvent, Category = "Unit Status")
    FText GetPassiveText() const;
    virtual FText GetPassiveText_Implementation() const;

private:

    TWeakObjectPtr<ACombatantBase> Unit;

    // Track border state so priority (orange > green) can be enforced.
    bool bIsTargeted    = false;
    bool bIsEnemyActing = false;

    /** Builds the entire card layout in C++ and assigns the bound members, so
     *  no WBP layout is needed (empty the WBP and reparent to this class). */
    void BuildCardLayout();

    void RefreshBorderColor();

    FText BuildHPText()            const;
    FText BuildAPText()            const;
    FText BuildStatusEffectsText() const;
};
