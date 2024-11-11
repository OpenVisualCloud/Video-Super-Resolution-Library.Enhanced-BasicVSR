# iVSR Release Notes

# Release v24.05

## New and Changes in v24.05
- Updated iVSR plugin to FFmpeg n6.1.
- Enabled the ability to change model input shapes in the SDK.
- Enabled Y-frame input support.
- Updated Enhanced BasicVSR and Enhanced EDSR models to support reshape.
- Updated SVP-Basic and SVP-SE models with improved inference performance.
- Preview version of TSENet model support.
- Removed the "input_size" parameter from the ivsr_process() API.
- Added one build option to disable applying OpenVINO patches which are required to support Enhanced BasicVSR model.

## Bug Fixes
- Fixed the issue of improper calling of ivsr_process() for the first frame in the FFmpeg plugin.
- Refined the SDK initialization process in the FFmpeg plugin, and removed several misleading warnings.

## Known Limitations
- iVSR SDK is runing in sync mode only. Aync mode is not enabled yet.
- The patch-based solution feature is not enabled in the FFmpeg plugin yet. It can be enabled by setting vsr_sample's "reshape_values" option.
- Parallel inference on heterogeneous hardware is not enabled yet.
- Only 8-bit data is supported in vsr_sample or iVSR FFmpeg plugin.
- Enhanced BasicVSR model works only on OpenVINO 2022.3 with the patches applied.
- The Enhanced BasicVSR model supports only input sizes with widths that are divisible by 32.
