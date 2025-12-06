// Created by Hollywood Camera Work - Public Domain

#include "MotionMatchingPrep.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AnimationBlueprintLibrary.h"
#include "Animation/Skeleton.h"

// TODO:
//
// Reverting has not been maintained during development, and is basically defunct. I've been working
// on copied clips, and for re-running the animation modifier, I've simply been deleting the old
// modified clip and making new copies. Use are your own peril. Particularly, speed curves are not
// reverted, but I've seen all of reverting been wonky. I've simply not cared.
//
// There's very little input validation. Since the class is based around sampling the world
// positions of specific bones, a bone not existing simply results in a zero transform and a bad
// result. I don't know how to provide feedback in an animation modifier.
//
// This is currently built around the UE5 Manny/Quinn skeletons.

UMotionMatchingPrep::UMotionMatchingPrep()
{
}

void UMotionMatchingPrep::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
    BoneNames = {
        RootBoneName,
        PelvisBoneName,
        LeftThighBoneName,
        RightThighBoneName,
        Spine01BoneName,
        // Spine02BoneName, // Center of gravity calculation is disabled. Didn't meaningfully improve path
        // Spine03BoneName,
        // Neck01BoneName,
        LeftFootBoneName,
        RightFootBoneName,
        LeftBallBoneName,
        RightBallBoneName,
        LeftHandBoneName,
        RightHandBoneName,
    };

    if (!AnimationSequence) {
        UE_LOG(LogAnimation, Error, TEXT("MotionMatchingPrep: Invalid animation sequence"));
        return;
    }

    USkeleton* Skeleton = AnimationSequence->GetSkeleton();
    if (!Skeleton) {
        UE_LOG(LogAnimation, Error, TEXT("MotionMatchingPrep: No skeleton found"));
        return;
    }

    // Verify all bones exist in skeleton
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    for (const FName& BoneName : BoneNames) {
        const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
        if (BoneIndex == INDEX_NONE) {
            UE_LOG(LogAnimation, Error, TEXT("MotionMatchingPrep: Bone '%s' not found in skeleton"), *BoneName.ToString());
            return;
        }
    }

    int32 NumFrames;
    UAnimationBlueprintLibrary::GetNumFrames(AnimationSequence, NumFrames);

    if (NumFrames <= 0) {
        UE_LOG(LogAnimation, Warning, TEXT("MotionMatchingPrep: Animation has no frames"));
        return;
    }

    UE_LOG(LogAnimation, Log, TEXT("MotionMatchingPrep: Processing %d frames..."), NumFrames);

    // Get animation data controller
    IAnimationDataController& Controller = AnimationSequence->GetController();
    const IAnimationDataModel* DataModel = AnimationSequence->GetDataModel();

    // Open bracket for bulk modifications
    Controller.OpenBracket(NSLOCTEXT("TransferPelvisToRoot", "ApplyModifier", "Transfer Pelvis to Root"));

    // Clear stored data
    OriginalTransforms.Empty();

    //
    // INITIALIZE KEYLESS BONES
    //

    // Iterate through all bones
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); BoneIndex++) {
        FName BoneName = RefSkeleton.GetBoneName(BoneIndex);

        // Check if bone already has a track (has keys)
        if (!DataModel->IsValidBoneTrackName(BoneName)) {
            // Get the bone's current transform at frame 0
            FTransform BoneTransform = DataModel->GetBoneTrackTransform(BoneName, FFrameNumber(0));

            // Set a key at time 0 with the current value
            TArray<FVector3f> PosKeys = { FVector3f(BoneTransform.GetLocation()) };
            TArray<FQuat4f> RotKeys = { FQuat4f(BoneTransform.GetRotation()) };
            TArray<FVector3f> ScaleKeys = { FVector3f(BoneTransform.GetScale3D()) };

            Controller.SetBoneTrackKeys(BoneName, PosKeys, RotKeys, ScaleKeys);
        }
    }

    //
    // TRANSFER SMOOTHED PELVIS TRANSLATION/ROTATION TO ROOT, AND USE THE NORMAL OF THREE HIP BONES
    // AS THE FACING DIRECTION
    //

    // Build new key arrays by sampling every frame
    TArray<FVector3f> RootPositions;
    TArray<FQuat4f> RootRotations;
    TArray<FVector3f> RootScales;

    TArray<FVector3f> PelvisPositions;
    TArray<FQuat4f> PelvisRotations;
    TArray<FVector3f> PelvisScales;

    TArray<FVector3f> IkLeftFootPositions;
    TArray<FQuat4f> IkLeftFootRotations;
    TArray<FVector3f> IkLeftFootScales;

    TArray<FVector3f> IkRightFootPositions;
    TArray<FQuat4f> IkRightFootRotations;
    TArray<FVector3f> IkRightFootScales;

    TArray<FVector3f> IkLeftHandPositions;
    TArray<FQuat4f> IkLeftHandRotations;
    TArray<FVector3f> IkLeftHandScales;

    TArray<FVector3f> IkRightHandPositions;
    TArray<FQuat4f> IkRightHandRotations;
    TArray<FVector3f> IkRightHandScales;

    // Reserve space
    RootPositions.Reserve(NumFrames);
    RootRotations.Reserve(NumFrames);
    RootScales.Reserve(NumFrames);

    PelvisPositions.Reserve(NumFrames);
    PelvisRotations.Reserve(NumFrames);
    PelvisScales.Reserve(NumFrames);

    IkLeftFootPositions.Reserve(NumFrames);
    IkLeftFootRotations.Reserve(NumFrames);
    IkLeftFootScales.Reserve(NumFrames);

    IkRightFootPositions.Reserve(NumFrames);
    IkRightFootRotations.Reserve(NumFrames);
    IkRightFootScales.Reserve(NumFrames);

    IkLeftHandPositions.Reserve(NumFrames);
    IkLeftHandRotations.Reserve(NumFrames);
    IkLeftHandScales.Reserve(NumFrames);

    IkRightHandPositions.Reserve(NumFrames);
    IkRightHandRotations.Reserve(NumFrames);
    IkRightHandScales.Reserve(NumFrames);

    UE_LOG(LogTemp, Log, TEXT("Processing animation modifier"));

    // Timing
    const float SequenceLength = AnimationSequence->GetPlayLength();
    const float FrameRate = (NumFrames > 1) ? (NumFrames - 1) / SequenceLength : 30.0f;
    const float FrameTime = 1.0f / FrameRate;

    // Create smoothing margins in actual frames, based on window size in seconds.
    TranslationSmoothingMinMargin = FrameRate * TranslationSmoothingMinSeconds / 2;
    TranslationSmoothingMaxMargin = FrameRate * TranslationSmoothingMaxSeconds / 2;

    UE_LOG(LogAnimation, Log, TEXT("MotionMatchingPrep: SequenceLength=%f, FrameRate=%f, FrameTime=%f"), SequenceLength, FrameRate, FrameTime);

    const TArray<TMap<FName, FTransform>> WorldTransforms = GetBoneWorldTransformsOverTime(AnimationSequence, NumFrames);

    // Get smoothed velocities of the pelvis bone for the whole sequence. We'll then analyze a
    // window around current time and use the lowest found velocity to scale the root smoothing
    // window size. This way, we can have a high degree of smoothing when we're far away from
    // starts/stops/turns, and a lower degree of smoothing when the character is taking detailed
    // actions.
    const int32 SmoothVelocityMargin = 0.41f * FrameRate;
    const auto SmoothVelocities = GetSmoothVelocitiesForBone(WorldTransforms, PelvisBoneName, SmoothVelocityMargin, FrameRate);

    // Convert world -> local and fill track key arrays
    for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex) {
        const TMap<FName, FTransform>& FrameWorld = WorldTransforms[FrameIndex];

        // Raw, unfiltered pelvis and root info
        const FTransform* RootWorld = FrameWorld.Find(RootBoneName);
        const FTransform* PelvisWorld = FrameWorld.Find(PelvisBoneName);
        const FVector PelvisWorldLocation = PelvisWorld->GetLocation();

        const float LowestVelocityInRange = LowestFloatValueInRange(SmoothVelocities, FrameIndex, TranslationSmoothingMaxMargin);

        const int32 RootSmoothing = FMath::GetMappedRangeValueClamped(
            FVector2D(TranslationVelocityMin, TranslationVelocityMax),
            FVector2D(TranslationSmoothingMinMargin, TranslationSmoothingMaxMargin),
            LowestVelocityInRange
        );

        UE_LOG(LogTemp, Log, TEXT("Frame %d: LowestVelocityInRange: %f, SmoothingWindowSize = %d"), FrameIndex, LowestVelocityInRange, RootSmoothing);

        // Smooth sample pelvis
        const FTransform SmoothPelvis = SmoothWorldTransformSingleBone(WorldTransforms, PelvisBoneName, FrameIndex, RootSmoothing);
        const FVector SmoothPelvisLocation = SmoothPelvis.GetLocation();
        const FQuat SmoothPelvisOrientation = SmoothPelvis.GetRotation();
        // const FTransform SmoothCenter = SmoothCenterOfGravity(WorldTransforms, FrameIndex, TranslationSmoothing);

        // Smooth sample average of balls of foot as an alternative root.
        const FTransform SmoothLeftBall = SmoothWorldTransformSingleBone(WorldTransforms, LeftBallBoneName, FrameIndex, RootSmoothing);
        const FTransform SmoothRightBall = SmoothWorldTransformSingleBone(WorldTransforms, RightBallBoneName, FrameIndex, RootSmoothing);
        const FTransform SmoothLeftFoot = SmoothWorldTransformSingleBone(WorldTransforms, LeftFootBoneName, FrameIndex, RootSmoothing);
        const FTransform SmoothRightFoot = SmoothWorldTransformSingleBone(WorldTransforms, RightFootBoneName, FrameIndex, RootSmoothing);
        const FVector SmoothFootCenter = (SmoothLeftBall.GetLocation() + SmoothRightBall.GetLocation() + SmoothLeftFoot.GetLocation() + SmoothRightFoot.GetLocation()) / 4;

        if (!RootWorld || !PelvisWorld) {
            // If either is missing, keep arrays in sync with a safe default and continue.
            // (You could also choose to break/ensure here.)
            RootPositions.Add(FVector3f::ZeroVector);
            RootRotations.Add(FQuat4f::Identity);
            RootScales.Add(FVector3f(1.0f, 1.0f, 1.0f));

            PelvisPositions.Add(FVector3f::ZeroVector);
            PelvisRotations.Add(FQuat4f::Identity);
            PelvisScales.Add(FVector3f(1.0f, 1.0f, 1.0f));
            UE_LOG(LogAnimation, Error, TEXT("MotionMatchingPrep: Missing bone data at frame %d"), FrameIndex);
            continue;
        }

#if true
        // Get the forward vector from the normal of thigh_r, thigh_l and spine_01. Then convert to pure yaw and assign to root.
        const FTransform SmoothThighR = SmoothWorldTransformSingleBone(WorldTransforms, RightThighBoneName, FrameIndex, RootSmoothing);
        const FTransform SmoothThighL = SmoothWorldTransformSingleBone(WorldTransforms, LeftThighBoneName, FrameIndex, RootSmoothing);
        const FTransform SmoothSpine01 = SmoothWorldTransformSingleBone(WorldTransforms, Spine01BoneName, FrameIndex, RootSmoothing);

        FVector ThighR = SmoothThighR.GetLocation();
        FVector ThighL = SmoothThighL.GetLocation();
        FVector Spine = SmoothSpine01.GetLocation();

        // Create edges from thigh_r to the other two points. Cross product to get normal
        // (right-hand rule: Edge2 x Edge1 to reverse direction)
        FVector Edge1 = ThighL - ThighR;  // thigh_r to thigh_l (points left)
        FVector Edge2 = Spine - ThighR;   // thigh_r to spine_01 (points up/forward)
        FVector Normal = FVector::CrossProduct(Edge2, Edge1);  // Swapped order to reverse
        Normal.Normalize();

        // Flatten on ground
        Normal.Z = 0.0f;
        Normal.Normalize();

        // Normal is hips/spine facing, flattened on ground
        FQuat FacingRotation;
        if (FinalFacingDirection == EMMFacingDirection::X) {
            float YawRadians = FMath::Atan2(Normal.Y, Normal.X);
            FacingRotation = FQuat(FVector::UpVector, YawRadians);
        }

        else if (FinalFacingDirection == EMMFacingDirection::Y) {
            // For Y-forward, rotate Normal 90 degrees: swap X/Y and negate
            float YawRadians = FMath::Atan2(-Normal.X, Normal.Y);
            FacingRotation = FQuat(FVector::UpVector, YawRadians);
        }

        else { // EMMFacingDirection::Z
            // For Z-forward, use Normal.Z component with X for pitch
            float YawRadians = FMath::Atan2(Normal.Y, Normal.X);
            FacingRotation = FQuat(FVector::UpVector, YawRadians);
        }

        // Create the root motion (original)
        // FTransform RootWorldShifted = *RootWorld;
        // const FVector RootPos = FVector(SmoothFootCenter.X, SmoothFootCenter.Y, 0.0f);
        // RootWorldShifted.SetLocation(RootPos);
        // RootWorldShifted.SetRotation(FacingRotation);

        // Create the root motion by combining forward motion of pelvis, orientation of hip, and
        // side-to-side motion of the foot average.
        FTransform RootWorldShifted = *RootWorld;
        const FVector SmoothFootCenterGround = FVector(SmoothFootCenter.X, SmoothFootCenter.Y, 0);
        const FVector RootPos = ComposeGroundMotion(SmoothPelvisLocation, SmoothFootCenterGround, FacingRotation);
        RootWorldShifted.SetLocation(RootPos);
        RootWorldShifted.SetRotation(FacingRotation);

        // Update root (absolute) and pelvis (relative)
        const FTransform RootLocal = RootWorldShifted;
        const FTransform PelvisLocal = PelvisWorld->GetRelativeTransform(RootWorldShifted);
#endif

#if false
        // Original, without changes
        const FTransform RootLocal = *RootWorld;
        const FTransform PelvisLocal = PelvisWorld->GetRelativeTransform(*RootWorld);
#endif
        // Push keys (convert to UE's float types used by the controller)
        RootPositions.Add(FVector3f(RootLocal.GetTranslation()));
        RootRotations.Add(FQuat4f(RootLocal.GetRotation()));
        RootScales.Add(FVector3f(RootLocal.GetScale3D()));

        PelvisPositions.Add(FVector3f(PelvisLocal.GetTranslation()));
        PelvisRotations.Add(FQuat4f(PelvisLocal.GetRotation()));
        PelvisScales.Add(FVector3f(PelvisLocal.GetScale3D()));

        // Reconstruct IK Foot positions. The IK bones are attached to root, but since we're now
        // shifting root around, we need to counter that movement in the IK Bones (which used to
        // have feet and hands relative to 0, 0, 0).
        const FTransform* LeftFootWorld = FrameWorld.Find(LeftFootBoneName);
        const FTransform NewLeftFootIk = LeftFootWorld->GetRelativeTransform(RootWorldShifted);
        IkLeftFootPositions.Add(FVector3f(NewLeftFootIk.GetTranslation()));
        IkLeftFootRotations.Add(FQuat4f(NewLeftFootIk.GetRotation()));
        IkLeftFootScales.Add(FVector3f(NewLeftFootIk.GetScale3D()));

        const FTransform* RightFootWorld = FrameWorld.Find(RightFootBoneName);
        const FTransform NewRightFootIk = RightFootWorld->GetRelativeTransform(RootWorldShifted);
        IkRightFootPositions.Add(FVector3f(NewRightFootIk.GetTranslation()));
        IkRightFootRotations.Add(FQuat4f(NewRightFootIk.GetRotation()));
        IkRightFootScales.Add(FVector3f(NewRightFootIk.GetScale3D()));

        // Reconstruct IK Hand positions. The Hand Gun bone is the real right hand (the right hand
        // bone is just a null transform off of right hand gun). So we set Hand Gun and Left Hand to
        // the world coordinates of their FK counterparts, and then redo their local coordinates off
        // of the IK Hand Root, which is just a null transform all the way down to the ultimate
        // root. But since we've shifted the root around with filtering, we'll get new local
        // transforms that will maintain the IK positions correctly. We do the right-hand first,
        // because the left hand is relative to the right hand for the IK bones.
        const FTransform* RightHandWorld = FrameWorld.Find(RightHandBoneName);
        const FTransform NewRightHandIk = RightHandWorld->GetRelativeTransform(RootWorldShifted);
        IkRightHandPositions.Add(FVector3f(NewRightHandIk.GetTranslation()));
        IkRightHandRotations.Add(FQuat4f(NewRightHandIk.GetRotation()));
        IkRightHandScales.Add(FVector3f(NewRightHandIk.GetScale3D()));

        const FTransform* LeftHandWorld = FrameWorld.Find(LeftHandBoneName);
        const FTransform NewLeftHandIk = LeftHandWorld->GetRelativeTransform(*RightHandWorld);
        IkLeftHandPositions.Add(FVector3f(NewLeftHandIk.GetTranslation()));
        IkLeftHandRotations.Add(FQuat4f(NewLeftHandIk.GetRotation()));
        IkLeftHandScales.Add(FVector3f(NewLeftHandIk.GetScale3D()));
    }

    // Now write the complete tracks back
    Controller.UpdateBoneTrackKeys(RootBoneName, FInt32Range(0, NumFrames), RootPositions, RootRotations, RootScales);
    Controller.UpdateBoneTrackKeys(PelvisBoneName, FInt32Range(0, NumFrames), PelvisPositions, PelvisRotations, PelvisScales);
    Controller.UpdateBoneTrackKeys(IkFootLBoneName, FInt32Range(0, NumFrames), IkLeftFootPositions, IkLeftFootRotations, IkLeftFootScales);
    Controller.UpdateBoneTrackKeys(IkFootRBoneName, FInt32Range(0, NumFrames), IkRightFootPositions, IkRightFootRotations, IkRightFootScales);
    Controller.UpdateBoneTrackKeys(IkHandGunBoneName, FInt32Range(0, NumFrames), IkRightHandPositions, IkRightHandRotations, IkRightHandScales);
    Controller.UpdateBoneTrackKeys(IkHandLBoneName, FInt32Range(0, NumFrames), IkLeftHandPositions, IkLeftHandRotations, IkLeftHandScales);

    //
    // CREATE FOOT SPEED CURVES
    //

    const TArray Feet = {LeftBallBoneName, RightBallBoneName};

    for (const auto& FootName : Feet) {
        // Arrays to store curve data
        TArray<float> CurveTimes;
        TArray<float> CurveValues;

        CurveTimes.Reserve(NumFrames);
        CurveValues.Reserve(NumFrames);

        // Calculate foot speed for each frame
        for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex) {
            const TMap<FName, FTransform>& CurrentFrame = WorldTransforms[FrameIndex];
            const FTransform* CurrentFootTransform = CurrentFrame.Find(FootName);

            if (!CurrentFootTransform) {
                CurveTimes.Add(FrameIndex * FrameTime);
                CurveValues.Add(0.0f);
                continue;
            }

            float Speed = 0.0f;

            // For the last frame, just use the same velocity as the previous frame
            if (FrameIndex == NumFrames - 1) {
                // Copy the previous frame's value (or set to 0 if this is the only frame)
                Speed = (CurveValues.Num() > 0) ? CurveValues.Last() : 0.0f;
            } else {
                // Normal case: calculate velocity to next frame
                const TMap<FName, FTransform>& NextFrame = WorldTransforms[FrameIndex + 1];
                const FTransform* NextFootTransform = NextFrame.Find(FootName);

                if (NextFootTransform) {
                    FVector Displacement = NextFootTransform->GetLocation() - CurrentFootTransform->GetLocation();
                    Speed = Displacement.Size() / FrameTime;
                }
            }

            CurveTimes.Add(FrameIndex * FrameTime);
            CurveValues.Add(Speed);
        }

        // Add the curve to the animation
        const FName CurveName = FName(FootName.ToString() + "_speed");

        // Check if curve already exists, if so remove it first
        if (DataModel->FindCurve(FAnimationCurveIdentifier(CurveName, ERawCurveTrackTypes::RCT_Float))) {
            Controller.RemoveCurve(FAnimationCurveIdentifier(CurveName, ERawCurveTrackTypes::RCT_Float));
        }

        // Add new curve
        FAnimationCurveIdentifier CurveId(CurveName, ERawCurveTrackTypes::RCT_Float);
        Controller.AddCurve(CurveId);

        // Set curve keys
        TArray<FRichCurveKey> Keys;
        Keys.Reserve(NumFrames);

        for (int32 i = 0; i < NumFrames; ++i) {
            FRichCurveKey Key;
            Key.Time = CurveTimes[i];
            Key.Value = CurveValues[i];
            Key.InterpMode = RCIM_Linear;
            Keys.Add(Key);
        }

        Controller.SetCurveKeys(CurveId, Keys);
    }

    // Close Bracket
    Controller.CloseBracket();

    UE_LOG(LogAnimation, Log, TEXT("MotionMatchingPrep: Successfully processed %d frames"), NumFrames);
}

void UMotionMatchingPrep::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
    // WARNING! Reverting hasn't been well maintained, and nobody knows if it works. I've been
    // deleting clips that failed processing and processed them again from an original copy.

    if (!AnimationSequence || OriginalTransforms.Num() == 0) {
        return;
    }

    // Get animation data controller
    IAnimationDataController& Controller = AnimationSequence->GetController();

    Controller.OpenBracket(NSLOCTEXT("TransferPelvisToRoot", "RevertModifier", "Revert Pelvis to Root Transfer"));

    // Rebuild tracks from stored originals
    TArray<FVector3f> RootPositions;
    TArray<FQuat4f> RootRotations;
    TArray<FVector3f> RootScales;

    TArray<FVector3f> PelvisPositions;
    TArray<FQuat4f> PelvisRotations;
    TArray<FVector3f> PelvisScales;

    const int32 NumFrames = OriginalTransforms.Num();

    RootPositions.Reserve(NumFrames);
    RootRotations.Reserve(NumFrames);
    RootScales.Reserve(NumFrames);
    PelvisPositions.Reserve(NumFrames);
    PelvisRotations.Reserve(NumFrames);
    PelvisScales.Reserve(NumFrames);

    // Restore original transforms
    for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex) {
        if (OriginalTransforms.Contains(FrameIndex)) {
            const FTransform& OriginalRoot = OriginalTransforms[FrameIndex].Key;
            const FTransform& OriginalPelvis = OriginalTransforms[FrameIndex].Value;

            RootPositions.Add(FVector3f(OriginalRoot.GetLocation()));
            RootRotations.Add(FQuat4f(OriginalRoot.GetRotation()));
            RootScales.Add(FVector3f(OriginalRoot.GetScale3D()));

            PelvisPositions.Add(FVector3f(OriginalPelvis.GetLocation()));
            PelvisRotations.Add(FQuat4f(OriginalPelvis.GetRotation()));
            PelvisScales.Add(FVector3f(OriginalPelvis.GetScale3D()));
        }
    }

    // Remove and re-add tracks
    const IAnimationDataModel* DataModel = AnimationSequence->GetDataModel();

    // Controller.AddBoneTrack(RootBoneName);
    // Controller.SetBoneTrackKeys(RootBoneName, RootPositions, RootRotations, RootScales);
    Controller.UpdateBoneTrackKeys(RootBoneName, FInt32Range(0, NumFrames), RootPositions, RootRotations, RootScales);

    // Controller.AddBoneTrack(PelvisBoneName);
    // Controller.SetBoneTrackKeys(PelvisBoneName, PelvisPositions, PelvisRotations, PelvisScales);
    Controller.UpdateBoneTrackKeys(PelvisBoneName, FInt32Range(0, NumFrames), PelvisPositions, PelvisRotations, PelvisScales);

    Controller.CloseBracket();

    OriginalTransforms.Empty();

    UE_LOG(LogAnimation, Log, TEXT("MotionMatchingPrep: Reverted changes"));
}

FTransform UMotionMatchingPrep::SmoothWorldTransformSingleBone(const TArray<TMap<FName, FTransform>>& WorldTransforms, FName Bone, const int32 FrameIndex, const int32 Margin)
{
    // Get the moving average of the bone's transform in a window of plus/minus Margin around
    // FrameIndex.

    const int32 TotalFrames = WorldTransforms.Num();
    const int32 StartFrame = FMath::Max(0, FrameIndex - Margin);
    const int32 EndFrame = FMath::Min(TotalFrames - 1, FrameIndex + Margin);

    FVector Location = FVector::ZeroVector;
    FVector Scale = FVector::ZeroVector;
    TArray<FQuat> Orientations;
    int32 Count = 0;

    for (int32 Index = StartFrame; Index <= EndFrame; ++Index) {
        const TMap<FName, FTransform>& FrameWorld = WorldTransforms[Index];
        const FTransform* BoneTransformPtr = FrameWorld.Find(Bone);
        check(BoneTransformPtr); // Assert if bone not found

        Location += BoneTransformPtr->GetLocation();
        Scale += BoneTransformPtr->GetScale3D();
        Orientations.Add(BoneTransformPtr->GetRotation());
        ++Count;
    }

    Location /= static_cast<float>(Count);
    Scale /= static_cast<float>(Count);
    const FQuat Orientation = AverageQuaternions(Orientations);

    return FTransform(Orientation, Location, Scale);
}

// FTransform UMotionMatchingPrep::SmoothCenterOfGravity(const TArray<TMap<FName, FTransform>>& WorldTransforms, const int32 FrameIndex, const int32 Margin)
// {
//     // Get the moving average of the multiple bones that make up the center of gravity, plus/minus
//     // Margin around FrameIndex.
//
//     const TArray TorsoBones = {
//         LeftThighBoneName,
//         RightThighBoneName,
//         Spine02BoneName,
//         Spine03BoneName,
//         Neck01BoneName,
//     };
//
//     FVector Location = FVector::ZeroVector;
//     FVector Scale = FVector::ZeroVector;
//     TArray<FQuat> Orientations;
//     int32 Count = 0;
//
//     for (const auto& BoneName : TorsoBones) {
//         const auto SmoothBone = SmoothTransform(WorldTransforms, BoneName, FrameIndex, Margin);
//
//         Location += SmoothBone.GetLocation();
//         Scale += SmoothBone.GetScale3D();
//         Orientations.Add(SmoothBone.GetRotation());
//         ++Count;
//     }
//
//     Location /= static_cast<float>(Count);
//     Scale /= static_cast<float>(Count);
//     const FQuat Orientation = AverageQuaternions(Orientations);
//
//     return FTransform(Orientation, Location, Scale);
// }

FQuat UMotionMatchingPrep::AverageQuaternions(const TArray<FQuat>& Quaternions)
{
    if (Quaternions.Num() == 0) {
        return FQuat::Identity;
    }

    if (Quaternions.Num() == 1) {
        return Quaternions[0];
    }

    // Start with the first quaternion as reference
    FQuat Average = Quaternions[0];

    // Accumulate the rest
    for (int32 i = 1; i < Quaternions.Num(); i++) {
        FQuat Q = Quaternions[i];

        // Ensure we're taking the shortest path (same hemisphere)
        // If dot product is negative, negate the quaternion
        if ((Average | Q) < 0.0f)  // | is dot product operator in Unreal
        {
            Q = Q * -1.0f;
        }

        // Accumulate
        Average.X += Q.X;
        Average.Y += Q.Y;
        Average.Z += Q.Z;
        Average.W += Q.W;
    }

    // Normalize the result
    Average.Normalize();

    return Average;
}

TMap<FName, FTransform> UMotionMatchingPrep::GetBoneWorldTransformsSingleFrame(UAnimSequence* AnimSequence, int32 FrameIndex, const TArray<FName>& RequestedBones)
{
    // Get the world transform for the bone names at the given frame by recursively adding
    // transforms.

    TMap<FName, FTransform> Results;

    if (!AnimSequence || RequestedBones.Num() == 0) {
        return Results;
    }

    USkeleton* Skeleton = AnimSequence->GetSkeleton();
    if (!Skeleton) {
        return Results;
    }

    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

    // Build set of target bone indices for quick lookup
    TSet<int32> TargetBoneIndices;
    for (const FName& BoneName : BoneNames) {
        int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
        if (BoneIndex != INDEX_NONE) {
            TargetBoneIndices.Add(BoneIndex);
        }
    }

    if (TargetBoneIndices.Num() == 0) {
        return Results;
    }

    // Build set of all bone indices we need to process
    // (target bones + all their ancestors up to root)
    TSet<int32> RequiredBoneIndices;
    for (int32 TargetIndex : TargetBoneIndices) {
        int32 CurrentIndex = TargetIndex;
        while (CurrentIndex != INDEX_NONE) {
            RequiredBoneIndices.Add(CurrentIndex);
            CurrentIndex = RefSkeleton.GetParentIndex(CurrentIndex);
        }
    }

    // Build a map of parent to children for required bones
    TMap<int32, TArray<int32>> ChildrenMap;
    for (int32 BoneIndex : RequiredBoneIndices) {
        int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
        if (ParentIndex != INDEX_NONE) {
            if (!ChildrenMap.Contains(ParentIndex)) {
                ChildrenMap.Add(ParentIndex, TArray<int32>());
            }
            ChildrenMap[ParentIndex].Add(BoneIndex);
        }
    }

    // Store component space transforms as we compute them
    TMap<int32, FTransform> ComponentSpaceTransforms;

    // Queue for breadth-first traversal: (BoneIndex, ParentComponentTransform)
    TQueue<TPair<int32, FTransform>> ProcessQueue;

    // Start with root bone (index 0)
    ProcessQueue.Enqueue(TPair<int32, FTransform>(0, FTransform::Identity));

    // Process bones breadth-first
    while (!ProcessQueue.IsEmpty()) {
        TPair<int32, FTransform> Current;
        ProcessQueue.Dequeue(Current);

        int32 BoneIndex = Current.Key;
        FTransform ParentComponentTransform = Current.Value;

        // Get this bone's local transform
        FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
        FTransform LocalTransform;
        UAnimationBlueprintLibrary::GetBonePoseForFrame(
            AnimSequence,
            BoneName,
            FrameIndex,
            true,
            LocalTransform
        );

        // Compute component space transform
        FTransform ComponentTransform = LocalTransform * ParentComponentTransform;
        ComponentSpaceTransforms.Add(BoneIndex, ComponentTransform);

        // If this is one of our target bones, store the result
        if (TargetBoneIndices.Contains(BoneIndex)) {
            Results.Add(BoneName, ComponentTransform);
        }

        // If we've found all target bones, we can early exit
        if (Results.Num() == TargetBoneIndices.Num()) {
            break;
        }

        // Queue child bones from our children map
        if (ChildrenMap.Contains(BoneIndex)) {
            for (int32 ChildIndex : ChildrenMap[BoneIndex]) {
                ProcessQueue.Enqueue(TPair<int32, FTransform>(ChildIndex, ComponentTransform));
            }
        }
    }

    return Results;
}

TArray<TMap<FName, FTransform>> UMotionMatchingPrep::GetBoneWorldTransformsOverTime(UAnimSequence* AnimSequence, int32 NumFrames)
{
    // Get all transforms for all frames for the tracked bones.

    TArray<TMap<FName, FTransform>> Result;

    for (int32 Index = 0; Index < NumFrames; ++Index) {
        Result.Add(GetBoneWorldTransformsSingleFrame(AnimSequence, Index, BoneNames));
    }

    return Result;
}

TArray<float> UMotionMatchingPrep::GetSmoothVelocitiesForBone(const TArray<TMap<FName, FTransform>>& WorldTransforms, const FName Bone, const int32 Margin, const int32 FrameRate)
{
    TArray<float> Result;
    FVector PreviousPosition = FVector::ZeroVector;

    for (int32 Frame = 0; Frame < WorldTransforms.Num(); ++Frame) {
        FVector Position = FVector::ZeroVector;
        const auto& FrameMap = WorldTransforms[Frame];

        if (const auto BoneTransform = FrameMap.Find(Bone)) {
            Position = BoneTransform->GetLocation();
        }

        const float Velocity = FrameRate * (Position - PreviousPosition).Size();
        Result.Add(Velocity);

        PreviousPosition = Position;
    }

    return GetSmoothedFloats(Result, Margin);
}

TArray<float> UMotionMatchingPrep::GetSmoothedFloats(const TArray<float>& Values, const int32 Margin)
{
    // Takes an arbitrary array of floats, and smoothes it with a rolling average window clamped to
    // valid index range.

    TArray<float> Result;
    const int32 NumValues = Values.Num();

    if (NumValues == 0) {
        return Result;
    }

    Result.Reserve(NumValues);

    for (int32 Index = 0; Index < NumValues; ++Index) {
        const int32 StartIndex = FMath::Max(0, Index - Margin);
        const int32 EndIndex = FMath::Min(NumValues - 1, Index + Margin);

        float Sum = 0.0f;
        int32 Count = 0;

        for (int32 i = StartIndex; i <= EndIndex; ++i) {
            Sum += Values[i];
            ++Count;
        }

        const float Average = Sum / static_cast<float>(Count);
        Result.Add(Average);
    }

    return Result;
}

float UMotionMatchingPrep::LowestFloatValueInRange(const TArray<float>& Values, const int32 FrameIndex, const int32 Margin)
{
    const auto NumValues = Values.Num();
    TOptional<float> Result;

    const int32 StartIndex = FMath::Max(0, FrameIndex - Margin);
    const int32 EndIndex = FMath::Min(NumValues - 1, FrameIndex + Margin);

    for (int32 i = StartIndex; i <= EndIndex; ++i) {
        const float Value = Values[i];
        if (!Result.IsSet() || Value < Result.GetValue()) {
            Result = Value;
        }
    }

    if (Result.IsSet()) {
        return Result.GetValue();
    } else {
        return 0;
    }
}

float UMotionMatchingPrep::HighestFloatValueInRange(const TArray<float>& Values, const int32 FrameIndex, const int32 Margin)
{
    const auto NumValues = Values.Num();
    TOptional<float> Result;

    const int32 StartIndex = FMath::Max(0, FrameIndex - Margin);
    const int32 EndIndex = FMath::Min(NumValues - 1, FrameIndex + Margin);

    for (int32 i = StartIndex; i <= EndIndex; ++i) {
        const float Value = Values[i];
        if (!Result.IsSet() || Value > Result.GetValue()) {
            Result = Value;
        }
    }

    if (Result.IsSet()) {
        return Result.GetValue();
    } else {
        return 999;
    }
}

int32 UMotionMatchingPrep::WindowSizeFromDivergence(const TArray<float>& Values, const int32 FrameIndex, const float PercentDivergence)
{
    // NOTE: Currently not used. Having a dynamic window size this way caused artifacts around
    // changes in window size, jumping as glitches in the root path.
    //
    // Detects a window size we can use for translation and rotation smoothing based on changes in
    // velocity. We start from a center value, and expand outwards until we find a velocity value
    // that diverges more than e.g. 10% (PercentDivergence = 0.1). This auto-sizes the window to
    // velocities that are more or less similar. Returns the margin that can be used for root
    // smoothing and facing smoothing. NOTE: This expects the Values to be valocity values that are
    // already smoothed. Without pre-smoothing, we can get some drastic edge effects as the window
    // size changes from frame to frame and we suddenly include frames in the smoothing or not. By
    // pre-smoothing, we ensure that there are no surprises lurking right outside the horizon that
    // suddenly change the averaging when we expand to include them.

    const auto NumValues = Values.Num();
    const float InitialValue = Values[FrameIndex];

    // When we stop, we want to return the previous spread value, not the current value where we
    // overshot.
    int32 PreviousSpread = 0;

    for (int32 Spread = 0; Spread < 1000; ++Spread) {
        const int32 LeftIndex = FrameIndex - Spread;
        if (LeftIndex >= 0) {
            const float ValueAtIndex = Values[LeftIndex];
            if (FMath::Abs(ValueAtIndex - InitialValue) / InitialValue > PercentDivergence) {
                return PreviousSpread;
            }
        }

        const int32 RightIndex = FrameIndex + Spread;
        if (RightIndex < NumValues) {
            const float ValueAtIndex = Values[RightIndex];
            if (FMath::Abs(ValueAtIndex - InitialValue) / InitialValue > PercentDivergence) {
                return PreviousSpread;
            }
        }

        PreviousSpread = Spread;
    }

    // Return a zero window size. I don't know exactly what this would mean.
    return 0;
}

FVector UMotionMatchingPrep::ComposeGroundMotion(const FVector& PelvisPos, const FVector& FootPlanePos, const FQuat& FootPlaneRot)
{
    // This function composes the ground motion from a combination of pelvis and foot motion. The
    // most reliable forward/backward movement comes from the pelvis bone, because it moves along
    // with the body's mass, and has realistic intertia. But this bone has sideways bobbing, and
    // doesn't do a good job of creating a path through the middle of the character's motion, which
    // is preferred for motion matching. Conversely, the most reliable lateral position comes from
    // an average of the foot bones (ball + foot in each side), which creates a sort of virtual bone
    // suspended between the feet. The sideways motion of this virtual bone is extremely stable, but
    // its forward motion speeds up and slows down along with the walking motion.
    //
    // This function expects a smoothed PelvisPos and a smoothed FootPlanePos and Rotation, which
    // has +X pointing forwards (will later be adapted to also accept +Y as the chosen forward
    // direction). It projects the pelvis down on to the foot plane, and then projects that plane
    // point onto the plane's X axis.

    // 1. Get the foot plane's normal (Z axis) and forward (X axis)
    FVector FootPlaneNormal = FootPlaneRot.GetAxisZ();
    // FVector FootPlaneForward = FootPlaneRot.GetAxisX();

    // New addition that properly honors the chosen forward direction. But to be clear, this worked
    // even by always getting the X axis, so mathematically, this is not so important. ChatGPT
    // explained it to me how this forward direction isn't used as deeply as one would thing. After
    // all, we're just piecing together a ground position by duct-taping different axis of several
    // inputs together.
    FVector FootPlaneForward;
    switch (FinalFacingDirection) {
    case EMMFacingDirection::Y:
        FootPlaneForward = FootPlaneRot.GetAxisY();
        break;
    case EMMFacingDirection::Z:
        // If you ever use Z, define what “forward on the plane” means here.
        FootPlaneForward = FootPlaneRot.GetAxisX(); // or some custom mapping
        break;
    case EMMFacingDirection::X:
    default:
        FootPlaneForward = FootPlaneRot.GetAxisX();
        break;
    }
    FootPlaneForward.Normalize();

    // 2. Project pelvis onto the foot plane
    FVector PelvisToPlane = PelvisPos - FootPlanePos;
    float DistanceToPlane = FVector::DotProduct(PelvisToPlane, FootPlaneNormal);
    FVector PelvisOnPlane = PelvisPos - (FootPlaneNormal * DistanceToPlane);

    // 3. Project pelvis onto the foot plane's X axis
    FVector PelvisOnPlaneDelta = PelvisOnPlane - FootPlanePos;
    float ForwardDistance = FVector::DotProduct(PelvisOnPlaneDelta, FootPlaneForward);

    // 4. Composed position on the plane's X axis
    FVector ComposedPosition = FootPlanePos + (FootPlaneForward * ForwardDistance);

    return ComposedPosition;
}
