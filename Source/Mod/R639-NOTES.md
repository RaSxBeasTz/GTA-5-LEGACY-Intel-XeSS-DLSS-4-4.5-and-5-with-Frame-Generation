# R639: depth separation candidate

R638 packet19602109: processed170/169, previous presented169, CPU estimation interval92.7253ms. Optical override9.0245%; finite vectors. Luma warp error camera .02796 versus FG .02206; changed pixels .07037 versus .01242. These metrics do not prove per-pixel visual correctness.

Captured character reversed depth around .065 versus wall .021-.025. Relative linear depth1/z gives sample character/wall separations24.21,29.06,32.91 below SDK default40. R639 sets minRelativeLinearDepthObjectSeparation to4. This is a candidate to improve foreground/background separation, not a proven sole cause or guaranteed fix. User must compare edge/occlusion behavior in motion.

Initial uninstalled candidate attempted a second constants update; SDK rejected it (SL27). That block was removed. Final build sets this field in the single normal constants update. NVIDIA SR source does not consume this field. Custom NR inputs, camera matrices, motion resources, depth values, optical gates and x6 are unchanged. Protected-source lock passed. No added GPU wait.

Primary references: https://github.com/NVIDIA-RTX/Streamline/blob/main/include/sl_consts.h ; https://github.com/NVIDIA-RTX/Streamline/blob/main/source/plugins/sl.common/commonEntry.cpp ; https://github.com/NVIDIA-RTX/Streamline/blob/main/source/plugins/sl.dlss/dlssEntry.cpp . Installed pinned headers document default40 and1/depth for inverted depth.

Final standalone test: 4K Quality,100/100 NR,100/100 full x6,600 presents,exit0. Threshold4 with one constants update logged. Native and WinUI builds passed; WinUI0 warnings/errors. Installed through backend with runtime backup20261029-220331-482-runtime-418712; settings preserved NVIDIA DLSS5/4K Quality/x6. F11 capture retained. No automatic GTA launch. Compare same indoor movement, press F11 if trails remain.
