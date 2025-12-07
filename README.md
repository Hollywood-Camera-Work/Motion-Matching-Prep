# Unreal Motion Matching Prep Tool

Animations used for Motion Matching in Unreal Engine can't simply project the pelvis down to root and call it root motion.

Motion Matching works by matching poses to trajectories, and the trajectory is always nice and pretty. The nicer and prettier the root motion is, the better Pose Search is at finding poses, and the better is the quality of the the foot contact, because the animation's root motion is similar to the smooth capsule movement of the character.

**Disclaimer**: *This code is good enough for what we needed, but it's not a full product. There's little input validation, it has only been tested on Manny/Quinn skeletons, and the reverting of Animation Modifiers may or may not work.*

## Pelvis Projection == Noisy Root Motion

When root motion is synthesized by just projecting the pelvis on the ground, the motion is very noisy, as the actor's center of gravity shifts around, and the speed changes during the walk cycle. This results in foot sliding in motion matching, because wobbly motion is played back on a character moving in a straight line, which requires more heavy-handed IK foot planting. Pose Search is then also trying to match straight character motion with wobbly animation sequences, resulting in poorer matches.

(Click image to play on YouTube)

[![image](https://github.com/user-attachments/assets/153291d3-b6b2-4294-9910-a2c52e9082bb)](https://www.youtube.com/watch?v=RfWo7364CKs)

## Proper Root Smoothing

The ideal root motion for motion matching is hand-painted, because it matches how the character actually moves in the game as well as the trajectory.

But selective smoothing is nearly as good. When the character is moving fast, we smooth the root a lot, and reduce the smoothing around starting/stopping/turning. We generate the center of gravity by averaging select foot bones, we generate the orientation from a cluster of bones so we get the actual skeleton facing-direction, and we also smooth the orientation.

(Click image to play on YouTube)

[![image](https://github.com/user-attachments/assets/56557d01-f407-4f9b-84cf-cd5e24560e43)](https://www.youtube.com/watch?v=MY1dHGu3TJ4)

## Usage

* Copy your animation clips to their destination.
* Add an Animation Modifier -> MotionMatchingPrep.
* If you're in a Manny/Quinn skeleton, Apply on default settings. Otherwise, you can specify bone names and smoothing factors.
* Remember to right-click clip, Asset Actions -> Edit Selection in Property Matrix, and enable "Enable Root Motion" and "Force Root Lock".

## What It Does

The goal is to smooth heavily while the character is moving, and reduce smoothing when the character is starting/stopping/turning.

The algorithm creates a velocity table, and chooses the lowest velocity in a given window around current time as "the velocity". This is used to reduce smoothing when slow and more detailed motion is in the near past or future. This is the Root Smoothing factor.

We now compose a root from disparate elements: We get an average of the smoothed ball and foot joints in each foot. The center point between the feet is better at determining the center of mass. We remove the Z component so it's ground motion. This will need work if we ever have non-level motion.

We get a forward vector from the normal of the thigh_l, thigh_r and spine_01, which form a triangle that's oriented more or less forwards. We project it on the ground and extract only the ground-level forward orientation (still smoothed by RootSmoothing).

We finally compose the root motion from a combination of pelvis and foot motion. The most reliable forward/backward movement comes from the pelvis bone, because it moves along
with the body's mass, and has realistic intertia. But this bone has sideways bobbing, and doesn't do a good job of creating a path through the middle of the character's motion, which is preferred for motion matching. Conversely, the most reliable lateral position comes from an average of the foot bones (ball + foot in each side), which creates a sort of virtual bone suspended between the feet. The sideways motion of this virtual bone is extremely stable, but its forward motion speeds up and slows down along with the walking motion. We finagle these different data sources together to produce the final root motion. Orientation is also smoothed.

