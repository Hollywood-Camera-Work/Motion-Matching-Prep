// Created by Hollywood Camera Work - Public Domain

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "MotionMatchingPrep.generated.h"

UENUM()
enum class EMMFacingDirection
{
    X,
    Y,
    Z,
};

UCLASS()
class GAMEANIMATIONSAMPLE2_API UMotionMatchingPrep : public UAnimationModifier
{
    GENERATED_BODY()

public:
    UMotionMatchingPrep();

    virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;
    virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) override;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Root bone that will receive the smoothed translation."))
    FName RootBoneName = TEXT("root");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Pelvis bone that currently holds the actor translation."))
    FName PelvisBoneName = TEXT("pelvis");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "The normal of Left Thigh, Right Thing and First Spine Joint is used as the facing direction."))
    FName LeftThighBoneName = TEXT("thigh_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "The normal of Left Thigh, Right Thing and First Spine Joint is used as the facing direction."))
    FName RightThighBoneName = TEXT("thigh_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "The normal of Left Thigh, Right Thing and Spine_01 Joint is used as the facing direction."))
    FName Spine01BoneName = TEXT("spine_01");

    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Spine_02, Spine_03 and Neck_01 are used in center-of-gravity calculations."))
    // FName Spine02BoneName = TEXT("spine_02");
    //
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Spine_02, Spine_03 and Neck_01 are used in center-of-gravity calculations."))
    // FName Spine03BoneName = TEXT("spine_03");
    //
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Spine_02, Spine_03 and Neck_01 are used in center-of-gravity calculations."))
    // FName Neck01BoneName = TEXT("neck_01");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Axis to be the forward/facing direction of the root node."))
    EMMFacingDirection FinalFacingDirection = EMMFacingDirection::Y;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "A curve channel will record the Left Foot speed."))
    FName LeftFootBoneName = TEXT("foot_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "A curve channel will record the Right Foot speed."))
    FName RightFootBoneName = TEXT("foot_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Left/Right ball of foot are use as an alternative root."))
    FName LeftBallBoneName = TEXT("ball_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Left/Right ball of foot are use as an alternative root."))
    FName RightBallBoneName = TEXT("ball_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Left FK hand"))
    FName LeftHandBoneName = TEXT("hand_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Right FK hand"))
    FName RightHandBoneName = TEXT("hand_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Attached to ik_foot_root, which is a null transform and therefore root. We conform this to the FK hand position after shifting around the root."))
    FName IkFootLBoneName = TEXT("ik_foot_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Attached to ik_foot_root, which is a null transform and therefore root. We conform this to the FK hand position after shifting around the root."))
    FName IkFootRBoneName = TEXT("ik_foot_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Attached to IK Hand Run to define the left hand relative to the right."))
    FName IkHandLBoneName = TEXT("ik_hand_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Right hand bone, attached to ik_hand_gun with null transform. IK Hand Gun is the real right hand."))
    FName IkHandRBoneName = TEXT("ik_hand_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = ""))
    FName IkHandGunBoneName = TEXT("ik_hand_gun");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Translation velocities in this range are mapped to smoothing ranges. Value is in units/sec."))
    float TranslationVelocityMin = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "Translation velocities in this range are mapped to smoothing ranges. Value is in units/sec."))
    float TranslationVelocityMax = 25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "The window in seconds around current time to use for translation moving average."))
    float TranslationSmoothingMinSeconds = 0.083;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "The window in seconds around current time to use for translation moving average."))
    float TranslationSmoothingMaxSeconds = 0.41;

    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "The margin around current time to use for translation moving average. Window size is 2 * margin."))
    // int32 TranslationSmoothingMin = 10;
    //
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "The margin around current time to use for translation moving average. Window size is 2 * margin."))
    // int32 TranslationSmoothingMax = 50;
    // 
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ToolTip = "The margin around current time to use for orientation moving average. Window size is 2 * margin."))
    // int32 RotationSmoothing = 40;

private:
    FTransform SmoothWorldTransformSingleBone(const TArray<TMap<FName, FTransform>>& WorldTransforms, FName Bone, const int32 FrameIndex, const int32 Margin);
    // FTransform SmoothCenterOfGravity(const TArray<TMap<FName, FTransform>>& WorldTransforms, const int32 FrameIndex, const int32 Margin);
    FQuat AverageQuaternions(const TArray<FQuat>& Quaternions);
    TMap<FName, FTransform> GetBoneWorldTransformsSingleFrame(UAnimSequence* AnimSequence, int32 FrameIndex, const TArray<FName>& RequestedBones);
    TArray<TMap<FName, FTransform>> GetBoneWorldTransformsOverTime(UAnimSequence* AnimSequence, int32 NumFrames);
    TArray<float> GetSmoothVelocitiesForBone(const TArray<TMap<FName, FTransform>>& WorldTransforms, FName Bone, const int32 Margin, int32 FrameRate);
    TArray<float> GetSmoothedFloats(const TArray<float>& Values, const int32 Margin);
    float LowestFloatValueInRange(const TArray<float>& Values, const int32 FrameIndex, const int32 Margin);
    float HighestFloatValueInRange(const TArray<float>& Values, const int32 FrameIndex, const int32 Margin);
    int32 WindowSizeFromDivergence(const TArray<float>& Values, const int32 FrameIndex, const float PercentDivergence);
    FVector ComposeGroundMotion(const FVector& PelvisPos, const FVector& FootPlanePos, const FQuat& FootPlaneRot);
    int32 TranslationSmoothingMinMargin = 1; // Half the smoothing window size, adjusted to frame rate.
    int32 TranslationSmoothingMaxMargin = 1; // Half the smoothing window size, adjusted to frame rate.

    TMap<int32, TPair<FTransform, FTransform>> OriginalTransforms;
    TArray<FName> BoneNames;
};
