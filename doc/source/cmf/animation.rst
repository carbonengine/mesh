Animation System Guide
======================

This guide covers the animation system in the Carbon Mesh library (CMF), including how to work with skeletal animations, the ``cmf::AnimationPlayer`` class, and advanced animation features like blending and sequencing.

Overview
--------

The CMF animation system provides a comprehensive set of tools for:

* Loading and playing skeletal animations
* Sampling animation curves at specific times
* Blending between multiple poses
* Managing animation sequences
* Additive animation support
* Per-bone masking and weighted blending

Core Concepts
-------------

Skeleton Pose
^^^^^^^^^^^^^

A ``cmf::SkeletonPose`` represents the state of a skeleton at a specific point in time. It contains:

* A reference to the skeleton
* A vector of local bone transforms (one per bone)

To initialize a pose with the rest pose of a skeleton, use the ``cmf::RestPose()`` function:

.. code-block:: cpp

    cmf::SkeletonPose pose;
    cmf::RestPose(pose, skeleton);

Animation Curves
^^^^^^^^^^^^^^^^

Animations are composed of curves that define how values change over time. 
Curves contain list of keyframes, where each keyframe has a time and a value. 
The library supports various curve types based on the dimensionality of the values they represent.
The values for keyframe times and keyframe values are stored in separate arrays any may be 
represented in different formats (e.g., 16-bit or 32-bit) to optimize memory usage.

Basic Animation Playback
-------------------------

Using AnimationPlayer
^^^^^^^^^^^^^^^^^^^^^

The ``cmf::AnimationPlayer`` class is the primary interface for playing animations. It manages playback of a single animation on a skeleton.

.. code-block:: cpp

    #include <cmf/animation.h>
    #include <cmf/cmf.h>

    // Assume skeleton and animation are already loaded from a CMF file
    const cmf::Skeleton& skeleton = /* ... */;
    const cmf::Animation& animation = /* ... */;

    // Create an animation player
    cmf::AnimationPlayer player(skeleton, animation);

    // Initialize a pose with the rest pose
    // The pose needs to be fully initialized before sampling because the animation 
    // may only contain curves for a subsetof bones. Uninitialized bones will be left unchanged.
    cmf::SkeletonPose pose;
    cmf::RestPose(pose, skeleton);

    // Sample the animation at time = 1.5 seconds
    float currentTime = 1.5f;
    player.Sample(pose, currentTime);

Setting Playback Parameters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``cmf::AnimationPlayer`` provides several parameters to control playback:

.. code-block:: cpp

    // Set playback speed (default is 1.0)
    player.SetSpeed(1.5f);  // Play 50% faster

    // Set loop count (0 means infinite looping)
    player.SetLoopCount(3);  // Play 3 times
    player.SetLoopCount(cmf::AnimationPlayer::INFINITE_LOOP);  // Loop forever

    // Set custom start and stop times
    player.SetStartTime(0.5f);   // Start at 0.5 seconds
    player.SetStopTime(2.0f);    // Stop at 2.0 seconds

    // Control extrapolation behavior
    player.SetExtrapolateBefore(true);  // Sample before start time
    player.SetExtrapolateAfter(false);  // Don't sample after stop time

Querying Animation State
^^^^^^^^^^^^^^^^^^^^^^^^

You can query the state of an animation at any time:

.. code-block:: cpp

    float time = 5.0f;

    // Check if animation is active at the given time
    if (player.IsActive(time)) {
        // Get the duration left
        float timeRemaining = player.GetDurationLeft(time);

        // Get the current loop index
        int32_t loopIndex = player.GetLoopIndex(time);

        // Get the local time within the current loop
        float localTime = player.GetLocalTime(time);

        std::cout << "Animation active: " << timeRemaining << "s remaining\n";
        std::cout << "Loop " << loopIndex << ", local time: " << localTime << "\n";
    }

    // Get the duration of one loop
    float loopDuration = player.GetLoopDuration();

Sampling Animations
-------------------

Direct Sampling
^^^^^^^^^^^^^^^

For simple cases, you can sample animations directly without using ``cmf::AnimationPlayer``. Note that
using ``cmf::AnimationPlayer`` is more efficient for continious playback as it caches curve evaluations and
provides additional features like looping and time management, so direct sampling is typically only used for specific use cases.

.. code-block:: cpp

    // Sample animation at a specific time
    cmf::SkeletonPose pose;
    cmf::RestPose(pose, skeleton);

    // Sample requires the animation and its curves
    const cmf::Animation& animation = /* ... */;
    cmf::Span<cmf::AnimationCurve> curves = /* ... */;

    float time = 2.5f;
    cmf::SampleAnimation(pose, animation, curves, time);

Sampling Individual Curves
^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can also sample individual curves directly. Note that sampling quaternion curves will use spherical linear 
interpolation (slerp) for smooth rotation blending.

.. code-block:: cpp

    const cmf::AnimationCurve& curve = /* ... */;
    float time = 1.0f;

    // Sample based on curve dimensionality
    float scalarValue = cmf::SampleScalarCurve(curve, time);
    Vector2 vec2Value = cmf::SampleVector2Curve(curve, time);
    Vector3 vec3Value = cmf::SampleVector3Curve(curve, time);
    Vector4 vec4Value = cmf::SampleVector4Curve(curve, time);
    Quaternion quatValue = cmf::SampleQuaternionCurve(curve, time);

Animation Blending
------------------

Simple Pose Blending
^^^^^^^^^^^^^^^^^^^^

Blend between two poses using a linear interpolation factor:

.. code-block:: cpp

    cmf::SkeletonPose poseA, poseB, blendedPose;

    // Initialize poses (e.g., from different animations)
    cmf::RestPose(poseA, skeleton);
    cmf::RestPose(poseB, skeleton);

    // Sample two different animations
    player1.Sample(poseA, time);
    player2.Sample(poseB, time);

    // Blend between them (alpha = 0.0 gives poseA, alpha = 1.0 gives poseB)
    float alpha = 0.5f;
    cmf::BlendPoses(blendedPose, poseA, poseB, alpha);

Per-Bone Weighted Blending
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Bone masks allow you to specify blend weights for each bone individually, controlling which bones are affected by blending. 
This is useful for layering animations (e.g., upper body aiming while lower body walks).

Use bone masks to blend different parts of the skeleton independently:

.. code-block:: cpp

    // Extract bone weights from a bone mask
    cmf::BoneWeights upperBodyWeights = 
        cmf::ExtractBoneWeights(skeleton, "UpperBodyMask");

    // Blend poses with per-bone weights
    // This allows the upper body to use poseB while the lower body uses poseA
    cmf::BlendPoses(blendedPose, poseA, poseB, upperBodyWeights, alpha);

Additive Animation
^^^^^^^^^^^^^^^^^^

Additive animations are used to layer animations on top of a base animation:

.. code-block:: cpp

    cmf::SkeletonPose basePose, additivePose, resultPose;
    cmf::RestPose(basePose, skeleton);
    cmf::RestPose(additivePose, skeleton);

    // Sample the base pose (e.g., idle animation)
    basePlayer.Sample(basePose, time);

    // Sample the additive pose (e.g., breathing or recoil)
    additivePlayer.Sample(additivePose, time);

    // Get the additive base reference (usually first frame of additive animation)
    cmf::SkeletonPose additiveBasePose;
    cmf::RestPose(additiveBasePose, skeleton);
    additivePlayer.SampleAtLocalTime(additiveBasePose, 0.0f);

    // Blend additive animation onto the base
    float additiveStrength = 0.8f;
    cmf::BlendAdditivePose(resultPose, basePose, 
                          additiveBasePose, additivePose, additiveStrength);

Animation Sequencing
--------------------

Using AnimationSequencer
^^^^^^^^^^^^^^^^^^^^^^^^

The ``cmf::AnimationSequencer`` class manages multiple animations, playing them one after another:

.. code-block:: cpp

    // Create a sequencer for the skeleton
    cmf::AnimationSequencer sequencer(skeleton);

    // Play animations in sequence
    auto player1 = sequencer.PlayAnimation(walkAnimation);
    auto player2 = sequencer.PlayAnimation(runAnimation);

    // Configure each animation
    player1->SetLoopCount(2);  // Walk twice
    player2->SetLoopCount(cmf::AnimationPlayer::INFINITE_LOOP);  // Then run forever

    // Sample the active animation
    cmf::SkeletonPose pose;
    cmf::RestPose(pose, skeleton);
    sequencer.Sample(pose, currentTime);

    // Remove finished animations
    sequencer.RemoveFinishedAnimations(currentTime);

Managing Animations in a Sequencer
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    // Stop a specific animation
    sequencer.StopAnimation(player1);

    // Enumerate all active animations
    sequencer.EnumerateAnimations([](const std::shared_ptr<cmf::AnimationPlayer>& player) {
        std::cout << "Active animation at time: " 
                  << player->GetStartTime() << "\n";
    });

Computing World Transforms
---------------------------

After sampling animation poses, you'll typically need per-bone transforms relative to the rest pose:

.. code-block:: cpp

    // Sample the animation
    cmf::SkeletonPose pose;
    cmf::RestPose(pose, skeleton);
    player.Sample(pose, currentTime);

    // Compute world-space transformation matrices
    std::vector<Matrix> worldTransforms;
    cmf::ComputeWorldTransforms(worldTransforms, pose);

    std::vector<Matrix> skinningTransforms;
    skinningTransforms.resize(worldTransforms.size());
    for (size_t i = 0; i < worldTransforms.size(); ++i) {
        // Combine with inverse bind pose for skinning
        skinningTransforms[i] = skeleton.invBindTransforms[i] * worldTransforms[i];  
    }


Complete Example
----------------

Here's a complete example showing a typical animation workflow:

.. code-block:: cpp

    #include <cmf/animation.h>
    #include <cmf/cmf.h>
    #include <iostream>

    void UpdateAnimation(const cmf::Skeleton& skeleton, 
                        const cmf::Animation& animation, 
                        float deltaTime) {
        static float currentTime = 0.0f;
        currentTime += deltaTime;

        // Create animation player
        static cmf::AnimationPlayer player(skeleton, animation);

        // Configure playback
        player.SetLoopCount(cmf::AnimationPlayer::INFINITE_LOOP);
        player.SetSpeed(1.0f);

        // Initialize pose
        cmf::SkeletonPose pose;
        cmf::RestPose(pose, skeleton);

        // Sample animation
        if (player.Sample(pose, currentTime)) {
            // Compute world transforms for rendering
            std::vector<cmf::Matrix> worldTransforms;
            cmf::ComputeWorldTransforms(worldTransforms, pose);

            std::vector<Matrix> skinningTransforms;
            skinningTransforms.resize(worldTransforms.size());
            for (size_t i = 0; i < worldTransforms.size(); ++i) {
                // Combine with inverse bind pose for skinning
                skinningTransforms[i] = skeleton.invBindTransforms[i] * worldTransforms[i];  
            }

            // Use skinningTransforms for skinning...
            std::cout << "Animation sampled successfully\n";
        }
    }

Advanced Animation Example
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Combining multiple animation techniques:

.. code-block:: cpp

    void AdvancedAnimationExample(const cmf::Skeleton& skeleton,
                                 const cmf::Animation& walkAnim,
                                 const cmf::Animation& aimAnim,
                                 float time) {
        // Create players for different animations
        cmf::AnimationPlayer walkPlayer(skeleton, walkAnim);
        cmf::AnimationPlayer aimPlayer(skeleton, aimAnim);

        walkPlayer.SetLoopCount(cmf::AnimationPlayer::INFINITE_LOOP);
        aimPlayer.SetLoopCount(cmf::AnimationPlayer::INFINITE_LOOP);

        // Sample both animations
        cmf::SkeletonPose walkPose, aimPose;
        cmf::RestPose(walkPose, skeleton);
        cmf::RestPose(aimPose, skeleton);

        walkPlayer.Sample(walkPose, time);
        aimPlayer.Sample(aimPose, time);

        // Blend upper body aiming with lower body walking
        cmf::BoneWeights upperBodyWeights = 
            cmf::ExtractBoneWeights(skeleton, "UpperBodyMask");

        cmf::SkeletonPose finalPose;
        cmf::RestPose(finalPose, skeleton);

        // Use per-bone weights to blend animations
        float aimBlend = 0.8f;  // 80% aim animation on upper body
        cmf::BlendPoses(finalPose, walkPose, aimPose, 
                       upperBodyWeights, aimBlend);

        // Compute final world transforms
        std::vector<cmf::Matrix> worldTransforms;
        cmf::ComputeWorldTransforms(worldTransforms, finalPose);
    }

Animation Optimization
----------------------

The library provides tools to optimize animation data:

.. code-block:: cpp

    // Remove duplicate curves to reduce memory usage
    cmf::MemoryAllocator allocator;  // Your custom allocator
    cmf::RemoveDuplicateCurves(animation, allocator);

    // Optimize curve format for smaller memory footprint
    cmf::AnimationCurve& curve = /* ... */;
    float keyTolerance = 0.001f;
    float valueTolerance = 0.001f;
    cmf::OptimizeCurveFormat(curve, keyTolerance, valueTolerance, allocator);

Time Management
---------------

Clock Rebasing
^^^^^^^^^^^^^^

When using very long playback times, floating-point precision can become an issue. Use clock rebasing to reset time references:

.. code-block:: cpp

    // Rebase a single player
    float deltaTime = -1000.0f;  // Subtract 1000 seconds from internal clocks
    player.RebaseClocks(deltaTime);

    // Rebase all animation players globally
    cmf::RebaseAllAnimationPlayerClocks(deltaTime);

This is useful for long-running games or applications where time values grow very large.

Best Practices
--------------

1. **Always initialize poses**: Before sampling, initialize poses with ``RestPose()`` or a valid previous pose.

2. **Reuse AnimationPlayer instances**: Creating animation players has some overhead, so reuse them when possible.

3. **Use bone masks for partial blending**: This gives you fine control over which parts of the skeleton are affected by different animations.

4. **Profile animation performance**: Use the curve evaluation cache to optimize repeated evaluations.

5. **Validate animation data**: Ensure animations are properly loaded and validated before use.

6. **Consider additive animations**: Use additive animations for layered effects like breathing, recoil, or procedural adjustments.

7. **Manage time carefully**: For long-running applications, periodically rebase clocks to maintain floating-point precision.

Common Pitfalls
---------------

* **Not initializing poses**: Always initialize pose bone transforms before sampling.
* **Mismatched skeletons**: Ensure all poses and animations reference the same skeleton.
* **Invalid time values**: Check that sample times are within valid ranges or use extrapolation settings appropriately.
* **Forgetting to compute world transforms**: Local bone transforms need to be converted to world space for rendering.
* **Memory leaks with sequencers**: Remember to call ``RemoveFinishedAnimations()`` to clean up completed animations.

See Also
--------

* :doc:`usage` - Basic CMF usage guide
* :doc:`fileFormat` - CMF file format specification
