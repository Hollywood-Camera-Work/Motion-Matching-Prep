# Unreal Motion Matching Prep Tool

Animations used for motion matching in Unreal Engine can't simply project the pelvis down to root and call it root motion.

Motion Matching works by matching poses to trajectories, and the trajectory is always nice and pretty. The nicer and prettier the root motion is the better Pose Search is at finding poses.

Simple pelvis -> root motion is very noisy as the actors body shifts around. This results in foot sliding in Unreal's Motion Matching, because it expects clean root paths, ideally hand-painted, requiring heavy IK Foot Planting. But doing selective smoothing is almost as good, and automatic, and Foot Planting is nearly perfect right out of the gate. Then IK Foot Planting simply does the last, complete lock, resulting in more natural animation.

## Pelvis Projection == Noisy Root Motion

When the pelvis is just projected on the ground, the root motion suffers from speed changes and constant shifting left and right.

(Click image to play on YouTube)

[![image](https://github.com/user-attachments/assets/153291d3-b6b2-4294-9910-a2c52e9082bb)](https://www.youtube.com/watch?v=RfWo7364CKs)

## Proper Root Smoothing

With root smoothing, we get close to hand-drawn root motion, and then Pose Search is operating with animations that look like the Motion Matching Trajectory.

(Click image to play on YouTube)

[![image](https://github.com/user-attachments/assets/56557d01-f407-4f9b-84cf-cd5e24560e43)](https://www.youtube.com/watch?v=MY1dHGu3TJ4)

## Usage

* Copy your animation clips to their destination.
* Add an Animation Modifier -> MotionMatchingPrep.
* If you're in a Manny/Quinn skeleton, Apply on default settings. Otherwise, you can specify bone names and smoothing factors.
* Remember to right-click clip, Asset Actions -> Edit Selection in Property Matrix, and enable "Enable Root Motion" and "Force Root Lock".

## What It Does

The goal is to smooth heavily while the character is moving, and reducing smoothing when the character is starting/stopping/turning.

The algorithm creates a velocity table, and chooses the lowest velocity in a given window around current time as "the velocity". This is used to reduce smoothing when slow and more detailed motion is in the near past or future. This is the Root Smoothing factor.

We now compose a root from disparate elements: We get an average of the smoothed ball and foot joints in each foot. The center point between the feet is better at determining the center of mass. We remove the Z component so it's ground motion. This will need work if we ever have non-level motion.

We get a forward vector from the normal of the thigh_l, thigh_r and spine_01, which form a triangle that's oriented more or less forwards. We project it on the ground and extract only the ground-level forward orientation (still smoothed by RootSmoothing).

We finally compose the root motion from a combination of pelvis and foot motion. The most reliable forward/backward movement comes from the pelvis bone, because it moves along
with the body's mass, and has realistic intertia. But this bone has sideways bobbing, and doesn't do a good job of creating a path through the middle of the character's motion, which is preferred for motion matching. Conversely, the most reliable lateral position comes from an average of the foot bones (ball + foot in each side), which creates a sort of virtual bone suspended between the feet. The sideways motion of this virtual bone is extremely stable, but its forward motion speeds up and slows down along with the walking motion. We finagle these different data sources together to produce the final root motion. Orientation is also smoothed.

