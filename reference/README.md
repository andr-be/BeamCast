# Reference A-Scan Examples

This folder contains reference A-scan images from real ultrasonic testing equipment for validation and tuning of the BeamCast simulator.

## Format

Each example should include:
- **Image file** (PNG/JPG)
- **Annotation file** (same name with `.txt` extension)

## Annotation Format

```
Equipment: [Make/Model]
Frequency: [MHz]
Gain: [dB]
Range: [μs]
Delay: [μs]
Rectification: [RF/Half-wave/Envelope]
Material: [e.g., "20mm steel block"]
Test setup: [e.g., "Straight beam, contact testing"]
Notes:
- Initial pulse shows ~3 cycles ringdown
- Back wall echo at 7 μs
- Noise floor barely visible at 40dB
```

## Examples to Include

- [ ] Straight beam thickness gauge (steel)
- [ ] Angle beam weld inspection
- [ ] High gain showing noise floor
- [ ] Saturated echo (gain too high)
- [ ] RF mode vs Rectified comparison
- [ ] Dead zone visualization

Add your reference images here for iterative A-scan rendering improvements!
