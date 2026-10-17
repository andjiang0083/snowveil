# Changelog

All notable milestones. Version numbers follow a small-patch rhythm — visual and
feature work is folded into patch-level bumps, not major jumps.

## v0.0.3a
- Quaternion attitude: pure gyro integration for free 360° rotation, no snap points or spring-back
- Spherical snow field: 1,500 flakes centred on the camera — snow visible in every direction
- Per-flake brightness (0.55–1.0) × depth falloff for layered depth
- BtnA view lock (faint LOCK hint), shake-storm unaffected
- Auto re-centering after 2 s still (≈7 s glide, cancels on movement)
- Shake-triggered blizzard (10 s) kept from the previous release

## v0.0.2b
- Radical subtraction: pure black background + snow + storm only

## v0.0.2a
- Shake triggers a blizzard: depth-coupled wind, faster falling, snow lifted by the gust

## v0.0.2
- Performance: precomputed colour LUT, direct sprite-memory writes, real-time-driven speeds
- Fixed byte-order bug when writing ST7789 sprite memory directly

## v0.0.1b
- Removed battery HUD for a cleaner picture; 40 large foreground flakes drifting toward you

## v0.0.1
- First snow-night simulation
