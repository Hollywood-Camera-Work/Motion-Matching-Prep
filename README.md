# Unreal Motion Matching Prep Tool

Animations used for motion matching in Unreal Engine can't simply project the pelvis down to root and call it root motion.

Motion Matching works by matching poses to trajectories, and the trajectory is always nice and pretty. The nicer and prettier the root motion is the better Pose Search is at finding poses. Simple pelvis -> root motion is very noisy as the actors body shifts around. Ideally, root motion should be hand-painted. But doing selective smoothing is almost as good, and automatic.

## Pelvis Projection == Noisy Root Motion

When the pelvis is just projected on the ground, the root motion suffers from speed changes and constant shifting left and right:

![image-20251206183018113](C:\Users\perho\AppData\Roaming\Typora\typora-user-images\image-20251206183018113.png)



There are unique requirements on using motion data for motion matching. The motion matching Trajectory is always a nice and smooth path, but typical root motion from real-world motion clips is messy. It's usually obtained by projecting the pelvis down to the ground, but this is still highly irregular because actors shift weight from foot to foot as they walk (creating a sine wave pattern on the floor), and they slow down and speed up during a single walk cycle. Pose Search will find the best matches if the root motion in the clip looks as much as possible like the Trajectory. The big picture is selective smoothing. We're trying to smooth out root motion and rotation when the character is moving, and reduce smoothing at lower velocities.
