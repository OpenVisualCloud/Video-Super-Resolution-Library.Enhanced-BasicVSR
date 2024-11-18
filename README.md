# Enhanced BasicVSR (iVSR)

[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/OpenVisualCloud/Video-Super-Resolution-Library.Enhanced-BasicVSR/badge)](https://api.securityscorecards.dev/projects/github.com/OpenVisualCloud/Video-Super-Resolution-Library.Enhanced-BasicVSR)
[![Dependency Review](https://github.com/OpenVisualCloud/Video-Super-Resolution-Library.Enhanced-BasicVSR/actions/workflows/dependency-review.yml/badge.svg)](https://github.com/OpenVisualCloud/Video-Super-Resolution-Library.Enhanced-BasicVSR/actions/workflows/dependency-review.yml)
[![CodeQL](https://github.com/OpenVisualCloud/Video-Super-Resolution-Library.Enhanced-BasicVSR/actions/workflows/codeql.yml/badge.svg)](https://github.com/OpenVisualCloud/Video-Super-Resolution-Library.Enhanced-BasicVSR/actions/workflows/codeql.yml)
[![License](https://img.shields.io/badge/license-BSD_3_Clause-stable.svg)](https://github.com/OpenVisualCloud/Video-Super-Resolution-Library.Enhanced-BasicVSR/blob/master/LICENSE.md)
[![Contributions](https://img.shields.io/badge/contributions-welcome-blue.svg)](https://github.com/OpenVisualCloud/Video-Super-Resolution-Library.Enhanced-BasicVSR/wiki)
[![Ubuntu-DockerFile-Build](https://github.com/OpenVisualCloud/Video-Super-Resolution-Library.Enhanced-BasicVSR/actions/workflows/ubuntu-build-docker.yml/badge.svg)](https://github.com/OpenVisualCloud/Video-Super-Resolution-Library.Enhanced-BasicVSR/actions/workflows/ubuntu-build-docker.yml)


Video super resolution (VSR) is widely used in AI media enhancement domain to 
convert low-resolution video to high-resolution. 


BasicVSR is a public AI-based VSR algorithm. 
For details of public BasicVSR, check out the [paper](https://arxiv.org/pdf/2012.02181.pdf). 

We have enhanced the public model to achieve better visual quality and less computational complexity, named Enhanced BasicVSR.
The performance of Enhanced BasicVSR inference has also been optimized for Intel GPU.
Now, 2x Enhanced BasicVSR can be run on both Intel CPU and Intel Data Center GPU Flex 170 (*aka* ATS-M1 150W) with OpenVINO and FFmpeg.


## How to evaluate

Please expect `pre-production` quality of current solution.

### Get models
Please [contact us](mailto:beryl.xu@intel.com) for FP32/INT8 Enhanced BasicVSR models. Will provide links to download the models soon.

### Run with OpenVINO
Refer to the guide [here](ivsr_gpu_opt/README.md).

### Run with FFmpeg
Refer to the guide [here](ivsr_ffmpeg_plugin/README.md).


## License

Enhanced BasicVSR is licensed under the BSD 3-clause license. See [LICENSE](LICENSE.md) for details.

