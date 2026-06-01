#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "RosterWidget.generated.h"

class URosterSubsystem;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class UWidgetSwitcher;
class UBorder;
struct FPartyMemberRecord;
enum class EPartyAssignment : uint8;

/** What a roster button does when clicked. */
UENUM()
enum class ERosterAction : uint8
{
    AssignParty1,
    AssignParty2,
    AssignBench,
    OpenDetails,
    BackToRoster,
    Close,
};

/**
 * URosterActionButton
 *
 * Button that remembers which party-member record it acts on (by index) and
 * which action it performs, so one handler routes every click.
 */
UCLASS()
class JRPGCOMBATTESTING_API URosterActionButton : public UButton
{
    GENERATED_BODY()

public:
    int32 MemberIndex = -1;
    ERosterAction Action = ERosterAction::Close;
    TFunction<void(int32, ERosterAction)> OnActionClicked;

    UFUNCTION()
    void HandleClicked();
};

/**
 * URosterWidget
 *
 * Full-screen party-management screen opened with Tab. Reads the PERSISTENT
 * party records from URosterSubsystem, so it is populated everywhere — Level,
 * Open World, and Camp — not just where combatant actors are placed.
 *
 * Page 0 (Roster): a card per member (name / level / HP / party tag) with
 *   [P1] [P2] [Bench] [Details] buttons, plus a footer showing party counts and
 *   the shared heal/revive/AP charge pool.
 * Page 1 (Detail): read-only stats + loadout for the selected member. Gear
 *   switching lands in a later pass.
 */
UCLASS()
class JRPGCOMBATTESTING_API URosterWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    TFunction<void()> OnCloseRequested;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual bool NativeSupportsKeyboardFocus() const override { return true; }
    virtual FReply NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Key) override;

private:
    TObjectPtr<UWidgetSwitcher> Switcher;
    TObjectPtr<UVerticalBox>    RosterList;
    TObjectPtr<UVerticalBox>    DetailBox;
    TObjectPtr<UTextBlock>      PartyCountText;

    URosterSubsystem* GetRoster() const;

    void RefreshRoster();
    void ShowDetail(int32 MemberIndex);
    void HandleAction(int32 MemberIndex, ERosterAction Action);

    UBorder* BuildCharacterCard(int32 MemberIndex);

    void AddDetailHeader(const FPartyMemberRecord& Rec);
    void AddStatsBlock(const FPartyMemberRecord& Rec);
    void AddLoadoutBlock(const FPartyMemberRecord& Rec);

    static FSlateFontInfo MakeFont(int32 Size, bool bBold = false);
    static FText AssignmentLabel(EPartyAssignment Assignment);
    static const TCHAR* WeaponTierName(uint8 Tier);
    static const TCHAR* BuffedStatName(uint8 Stat);
};
