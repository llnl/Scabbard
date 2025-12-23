<!-- LTO Passes listed -->
LTO passes for device
```
BISECT: running pass (1) CrossDSOCFIPass on [module]
BISECT: running pass (2) OpenMPOptPass on [module]
BISECT: running pass (3) GlobalDCEPass on [module]
BISECT: running pass (4) InferFunctionAttrsPass on [module]
BISECT: running pass (5) CallSiteSplittingPass on __cxa_pure_virtual
BISECT: running pass (6) CallSiteSplittingPass on __cxa_deleted_virtual
BISECT: running pass (7) CallSiteSplittingPass on _Z10matrix_mulPdS_S_
BISECT: running pass (8) CallSiteSplittingPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (9) CallSiteSplittingPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (10) CallSiteSplittingPass on __ockl_get_group_id
BISECT: running pass (11) CallSiteSplittingPass on _ZN4dim3C2Ejjj
BISECT: running pass (12) CallSiteSplittingPass on __ockl_get_local_id
BISECT: running pass (13) PGOIndirectCallPromotion on [module]
BISECT: running pass (14) IPSCCPPass on [module]
BISECT: running pass (15) CalledValuePropagationPass on [module]
BISECT: running pass (16) PostOrderFunctionAttrsPass on (__cxa_pure_virtual)
BISECT: running pass (17) PostOrderFunctionAttrsPass on (__cxa_deleted_virtual)
BISECT: running pass (18) PostOrderFunctionAttrsPass on (_Z10matrix_mulPdS_S_)
BISECT: running pass (19) PostOrderFunctionAttrsPass on (__ockl_get_group_id)
BISECT: running pass (20) PostOrderFunctionAttrsPass on (_ZN4dim3C2Ejjj)
BISECT: running pass (21) PostOrderFunctionAttrsPass on (__ockl_get_local_id)
BISECT: running pass (22) PostOrderFunctionAttrsPass on (scabbard.trace.device.trace_append$mem)
BISECT: running pass (23) PostOrderFunctionAttrsPass on (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (24) ReversePostOrderFunctionAttrsPass on [module]
BISECT: running pass (25) GlobalSplitPass on [module]
BISECT: running pass (26) WholeProgramDevirtPass on [module]
BISECT: running pass (27) GlobalOptPass on [module]
BISECT: running pass (28) PromotePass on __cxa_pure_virtual
BISECT: running pass (29) PromotePass on __cxa_deleted_virtual
BISECT: running pass (30) PromotePass on _Z10matrix_mulPdS_S_
BISECT: running pass (31) PromotePass on scabbard.trace.device.trace_append$mem
BISECT: running pass (32) PromotePass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (33) PromotePass on __ockl_get_group_id
BISECT: running pass (34) PromotePass on _ZN4dim3C2Ejjj
BISECT: running pass (35) PromotePass on __ockl_get_local_id
BISECT: running pass (36) ConstantMergePass on [module]
BISECT: running pass (37) DeadArgumentEliminationPass on [module]
BISECT: running pass (38) InstCombinePass on __cxa_pure_virtual
BISECT: running pass (39) AggressiveInstCombinePass on __cxa_pure_virtual
BISECT: running pass (40) AMDGPUUseNativeCallsPass on __cxa_pure_virtual
BISECT: running pass (41) AMDGPUSimplifyLibCallsPass on __cxa_pure_virtual
BISECT: running pass (42) InstCombinePass on __cxa_deleted_virtual
BISECT: running pass (43) AggressiveInstCombinePass on __cxa_deleted_virtual
BISECT: running pass (44) AMDGPUUseNativeCallsPass on __cxa_deleted_virtual
BISECT: running pass (45) AMDGPUSimplifyLibCallsPass on __cxa_deleted_virtual
BISECT: running pass (46) InstCombinePass on _Z10matrix_mulPdS_S_
BISECT: running pass (47) AggressiveInstCombinePass on _Z10matrix_mulPdS_S_
BISECT: running pass (48) AMDGPUUseNativeCallsPass on _Z10matrix_mulPdS_S_
BISECT: running pass (49) AMDGPUSimplifyLibCallsPass on _Z10matrix_mulPdS_S_
BISECT: running pass (50) InstCombinePass on scabbard.trace.device.trace_append$mem
BISECT: running pass (51) AggressiveInstCombinePass on scabbard.trace.device.trace_append$mem
BISECT: running pass (52) AMDGPUUseNativeCallsPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (53) AMDGPUSimplifyLibCallsPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (54) InstCombinePass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (55) AggressiveInstCombinePass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (56) AMDGPUUseNativeCallsPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (57) AMDGPUSimplifyLibCallsPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (58) InstCombinePass on __ockl_get_group_id
BISECT: running pass (59) AggressiveInstCombinePass on __ockl_get_group_id
BISECT: running pass (60) AMDGPUUseNativeCallsPass on __ockl_get_group_id
BISECT: running pass (61) AMDGPUSimplifyLibCallsPass on __ockl_get_group_id
BISECT: running pass (62) InstCombinePass on _ZN4dim3C2Ejjj
BISECT: running pass (63) AggressiveInstCombinePass on _ZN4dim3C2Ejjj
BISECT: running pass (64) AMDGPUUseNativeCallsPass on _ZN4dim3C2Ejjj
BISECT: running pass (65) AMDGPUSimplifyLibCallsPass on _ZN4dim3C2Ejjj
BISECT: running pass (66) InstCombinePass on __ockl_get_local_id
BISECT: running pass (67) AggressiveInstCombinePass on __ockl_get_local_id
BISECT: running pass (68) AMDGPUUseNativeCallsPass on __ockl_get_local_id
BISECT: running pass (69) AMDGPUSimplifyLibCallsPass on __ockl_get_local_id
BISECT: running pass (70) InlinerPass on (__cxa_pure_virtual)
BISECT: running pass (71) InlinerPass on (__cxa_pure_virtual)
BISECT: running pass (72) InlinerPass on (__cxa_deleted_virtual)
BISECT: running pass (73) InlinerPass on (__cxa_deleted_virtual)
BISECT: running pass (74) InlinerPass on (_Z10matrix_mulPdS_S_)
BISECT: running pass (75) InlinerPass on (_Z10matrix_mulPdS_S_)
BISECT: running pass (76) InlinerPass on (__ockl_get_group_id)
BISECT: running pass (77) InlinerPass on (__ockl_get_group_id)
BISECT: running pass (78) InlinerPass on (_ZN4dim3C2Ejjj)
BISECT: running pass (79) InlinerPass on (_ZN4dim3C2Ejjj)
BISECT: running pass (80) InlinerPass on (__ockl_get_local_id)
BISECT: running pass (81) InlinerPass on (__ockl_get_local_id)
BISECT: running pass (82) InlinerPass on (scabbard.trace.device.trace_append$mem)
BISECT: running pass (83) InlinerPass on (scabbard.trace.device.trace_append$mem)
BISECT: running pass (84) InlinerPass on (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (85) InlinerPass on (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (86) GlobalOptPass on [module]
BISECT: running pass (87) OpenMPOptPass on [module]
BISECT: running pass (88) GlobalDCEPass on [module]
BISECT: running pass (89) ArgumentPromotionPass on (__cxa_pure_virtual)
BISECT: running pass (90) ArgumentPromotionPass on (__cxa_deleted_virtual)
BISECT: running pass (91) ArgumentPromotionPass on (_Z10matrix_mulPdS_S_)
BISECT: running pass (92) ArgumentPromotionPass on (_ZN4dim3C2Ejjj)
BISECT: running pass (93) ArgumentPromotionPass on (scabbard.trace.device.trace_append$mem)
BISECT: running pass (94) ArgumentPromotionPass on (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (95) InstCombinePass on __cxa_pure_virtual
BISECT: running pass (96) AMDGPUUseNativeCallsPass on __cxa_pure_virtual
BISECT: running pass (97) AMDGPUSimplifyLibCallsPass on __cxa_pure_virtual
BISECT: running pass (98) ConstraintEliminationPass on __cxa_pure_virtual
BISECT: running pass (99) JumpThreadingPass on __cxa_pure_virtual
BISECT: running pass (100) SROAPass on __cxa_pure_virtual
BISECT: running pass (101) TailCallElimPass on __cxa_pure_virtual
BISECT: running pass (102) InstCombinePass on __cxa_deleted_virtual
BISECT: running pass (103) AMDGPUUseNativeCallsPass on __cxa_deleted_virtual
BISECT: running pass (104) AMDGPUSimplifyLibCallsPass on __cxa_deleted_virtual
BISECT: running pass (105) ConstraintEliminationPass on __cxa_deleted_virtual
BISECT: running pass (106) JumpThreadingPass on __cxa_deleted_virtual
BISECT: running pass (107) SROAPass on __cxa_deleted_virtual
BISECT: running pass (108) TailCallElimPass on __cxa_deleted_virtual
BISECT: running pass (109) InstCombinePass on _Z10matrix_mulPdS_S_
BISECT: running pass (110) AMDGPUUseNativeCallsPass on _Z10matrix_mulPdS_S_
BISECT: running pass (111) AMDGPUSimplifyLibCallsPass on _Z10matrix_mulPdS_S_
BISECT: running pass (112) ConstraintEliminationPass on _Z10matrix_mulPdS_S_
BISECT: running pass (113) JumpThreadingPass on _Z10matrix_mulPdS_S_
BISECT: running pass (114) SROAPass on _Z10matrix_mulPdS_S_
BISECT: running pass (115) TailCallElimPass on _Z10matrix_mulPdS_S_
BISECT: running pass (116) InstCombinePass on scabbard.trace.device.trace_append$mem
BISECT: running pass (117) AMDGPUUseNativeCallsPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (118) AMDGPUSimplifyLibCallsPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (119) ConstraintEliminationPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (120) JumpThreadingPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (121) SROAPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (122) TailCallElimPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (123) InstCombinePass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (124) AMDGPUUseNativeCallsPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (125) AMDGPUSimplifyLibCallsPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (126) ConstraintEliminationPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (127) JumpThreadingPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (128) SROAPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (129) TailCallElimPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (130) InstCombinePass on _ZN4dim3C2Ejjj
BISECT: running pass (131) AMDGPUUseNativeCallsPass on _ZN4dim3C2Ejjj
BISECT: running pass (132) AMDGPUSimplifyLibCallsPass on _ZN4dim3C2Ejjj
BISECT: running pass (133) ConstraintEliminationPass on _ZN4dim3C2Ejjj
BISECT: running pass (134) JumpThreadingPass on _ZN4dim3C2Ejjj
BISECT: running pass (135) SROAPass on _ZN4dim3C2Ejjj
BISECT: running pass (136) TailCallElimPass on _ZN4dim3C2Ejjj
BISECT: running pass (137) PostOrderFunctionAttrsPass on (__cxa_pure_virtual)
BISECT: running pass (138) PostOrderFunctionAttrsPass on (__cxa_deleted_virtual)
BISECT: running pass (139) PostOrderFunctionAttrsPass on (_Z10matrix_mulPdS_S_)
BISECT: running pass (140) PostOrderFunctionAttrsPass on (_ZN4dim3C2Ejjj)
BISECT: running pass (141) PostOrderFunctionAttrsPass on (scabbard.trace.device.trace_append$mem)
BISECT: running pass (142) PostOrderFunctionAttrsPass on (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (143) InvalidateAnalysisPass<llvm::AAManager> on __cxa_pure_virtual
BISECT: running pass (144) InvalidateAnalysisPass<llvm::AAManager> on __cxa_deleted_virtual
BISECT: running pass (145) InvalidateAnalysisPass<llvm::AAManager> on _Z10matrix_mulPdS_S_
BISECT: running pass (146) InvalidateAnalysisPass<llvm::AAManager> on scabbard.trace.device.trace_append$mem
BISECT: running pass (147) InvalidateAnalysisPass<llvm::AAManager> on scabbard.trace.device.trace_append$alloc
BISECT: running pass (148) InvalidateAnalysisPass<llvm::AAManager> on _ZN4dim3C2Ejjj
BISECT: running pass (149) OpenMPOptCGSCCPass on (__cxa_pure_virtual)
BISECT: running pass (150) OpenMPOptCGSCCPass on (__cxa_deleted_virtual)
BISECT: running pass (151) OpenMPOptCGSCCPass on (_Z10matrix_mulPdS_S_)
BISECT: running pass (152) OpenMPOptCGSCCPass on (_ZN4dim3C2Ejjj)
BISECT: running pass (153) OpenMPOptCGSCCPass on (scabbard.trace.device.trace_append$mem)
BISECT: running pass (154) OpenMPOptCGSCCPass on (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (155) LoopSimplifyPass on __cxa_pure_virtual
BISECT: running pass (156) LCSSAPass on __cxa_pure_virtual
BISECT: running pass (157) GVNPass on __cxa_pure_virtual
BISECT: running pass (158) MemCpyOptPass on __cxa_pure_virtual
BISECT: running pass (159) DSEPass on __cxa_pure_virtual
BISECT: running pass (160) MoveAutoInitPass on __cxa_pure_virtual
BISECT: running pass (161) MergedLoadStoreMotionPass on __cxa_pure_virtual
BISECT: running pass (162) LoopSimplifyPass on __cxa_pure_virtual
BISECT: running pass (163) LCSSAPass on __cxa_pure_virtual
BISECT: running pass (164) LoopDistributePass on __cxa_pure_virtual
BISECT: running pass (165) LoopVectorizePass on __cxa_pure_virtual
BISECT: running pass (166) InferAlignmentPass on __cxa_pure_virtual
BISECT: running pass (167) LoopUnrollPass on __cxa_pure_virtual
BISECT: running pass (168) WarnMissedTransformationsPass on __cxa_pure_virtual
BISECT: running pass (169) SROAPass on __cxa_pure_virtual
BISECT: running pass (170) InstCombinePass on __cxa_pure_virtual
BISECT: running pass (171) SimplifyCFGPass on __cxa_pure_virtual
BISECT: running pass (172) SCCPPass on __cxa_pure_virtual
BISECT: running pass (173) InstCombinePass on __cxa_pure_virtual
BISECT: running pass (174) BDCEPass on __cxa_pure_virtual
BISECT: running pass (175) SLPVectorizerPass on __cxa_pure_virtual
BISECT: running pass (176) VectorCombinePass on __cxa_pure_virtual
BISECT: running pass (177) InferAlignmentPass on __cxa_pure_virtual
BISECT: running pass (178) InstCombinePass on __cxa_pure_virtual
BISECT: running pass (179) LoopSimplifyPass on __cxa_pure_virtual
BISECT: running pass (180) LCSSAPass on __cxa_pure_virtual
BISECT: running pass (181) AlignmentFromAssumptionsPass on __cxa_pure_virtual
BISECT: running pass (182) AMDGPUUseNativeCallsPass on __cxa_pure_virtual
BISECT: running pass (183) AMDGPUSimplifyLibCallsPass on __cxa_pure_virtual
BISECT: running pass (184) JumpThreadingPass on __cxa_pure_virtual
BISECT: running pass (185) LoopSimplifyPass on __cxa_deleted_virtual
BISECT: running pass (186) LCSSAPass on __cxa_deleted_virtual
BISECT: running pass (187) GVNPass on __cxa_deleted_virtual
BISECT: running pass (188) MemCpyOptPass on __cxa_deleted_virtual
BISECT: running pass (189) DSEPass on __cxa_deleted_virtual
BISECT: running pass (190) MoveAutoInitPass on __cxa_deleted_virtual
BISECT: running pass (191) MergedLoadStoreMotionPass on __cxa_deleted_virtual
BISECT: running pass (192) LoopSimplifyPass on __cxa_deleted_virtual
BISECT: running pass (193) LCSSAPass on __cxa_deleted_virtual
BISECT: running pass (194) LoopDistributePass on __cxa_deleted_virtual
BISECT: running pass (195) LoopVectorizePass on __cxa_deleted_virtual
BISECT: running pass (196) InferAlignmentPass on __cxa_deleted_virtual
BISECT: running pass (197) LoopUnrollPass on __cxa_deleted_virtual
BISECT: running pass (198) WarnMissedTransformationsPass on __cxa_deleted_virtual
BISECT: running pass (199) SROAPass on __cxa_deleted_virtual
BISECT: running pass (200) InstCombinePass on __cxa_deleted_virtual
BISECT: running pass (201) SimplifyCFGPass on __cxa_deleted_virtual
BISECT: running pass (202) SCCPPass on __cxa_deleted_virtual
BISECT: running pass (203) InstCombinePass on __cxa_deleted_virtual
BISECT: running pass (204) BDCEPass on __cxa_deleted_virtual
BISECT: running pass (205) SLPVectorizerPass on __cxa_deleted_virtual
BISECT: running pass (206) VectorCombinePass on __cxa_deleted_virtual
BISECT: running pass (207) InferAlignmentPass on __cxa_deleted_virtual
BISECT: running pass (208) InstCombinePass on __cxa_deleted_virtual
BISECT: running pass (209) LoopSimplifyPass on __cxa_deleted_virtual
BISECT: running pass (210) LCSSAPass on __cxa_deleted_virtual
BISECT: running pass (211) AlignmentFromAssumptionsPass on __cxa_deleted_virtual
BISECT: running pass (212) AMDGPUUseNativeCallsPass on __cxa_deleted_virtual
BISECT: running pass (213) AMDGPUSimplifyLibCallsPass on __cxa_deleted_virtual
BISECT: running pass (214) JumpThreadingPass on __cxa_deleted_virtual
BISECT: running pass (215) LoopSimplifyPass on _Z10matrix_mulPdS_S_
BISECT: running pass (216) LCSSAPass on _Z10matrix_mulPdS_S_
BISECT: running pass (217) GVNPass on _Z10matrix_mulPdS_S_
BISECT: running pass (218) MemCpyOptPass on _Z10matrix_mulPdS_S_
BISECT: running pass (219) DSEPass on _Z10matrix_mulPdS_S_
BISECT: running pass (220) MoveAutoInitPass on _Z10matrix_mulPdS_S_
BISECT: running pass (221) MergedLoadStoreMotionPass on _Z10matrix_mulPdS_S_
BISECT: running pass (222) LoopSimplifyPass on _Z10matrix_mulPdS_S_
BISECT: running pass (223) LCSSAPass on _Z10matrix_mulPdS_S_
BISECT: running pass (224) LoopDistributePass on _Z10matrix_mulPdS_S_
BISECT: running pass (225) LoopVectorizePass on _Z10matrix_mulPdS_S_
BISECT: running pass (226) InferAlignmentPass on _Z10matrix_mulPdS_S_
BISECT: running pass (227) LoopUnrollPass on _Z10matrix_mulPdS_S_
BISECT: running pass (228) WarnMissedTransformationsPass on _Z10matrix_mulPdS_S_
BISECT: running pass (229) SROAPass on _Z10matrix_mulPdS_S_
BISECT: running pass (230) InstCombinePass on _Z10matrix_mulPdS_S_
BISECT: running pass (231) SimplifyCFGPass on _Z10matrix_mulPdS_S_
BISECT: running pass (232) SCCPPass on _Z10matrix_mulPdS_S_
BISECT: running pass (233) InstCombinePass on _Z10matrix_mulPdS_S_
BISECT: running pass (234) BDCEPass on _Z10matrix_mulPdS_S_
BISECT: running pass (235) SLPVectorizerPass on _Z10matrix_mulPdS_S_
BISECT: running pass (236) VectorCombinePass on _Z10matrix_mulPdS_S_
BISECT: running pass (237) InferAlignmentPass on _Z10matrix_mulPdS_S_
BISECT: running pass (238) InstCombinePass on _Z10matrix_mulPdS_S_
BISECT: running pass (239) LoopSimplifyPass on _Z10matrix_mulPdS_S_
BISECT: running pass (240) LCSSAPass on _Z10matrix_mulPdS_S_
BISECT: running pass (241) AlignmentFromAssumptionsPass on _Z10matrix_mulPdS_S_
BISECT: running pass (242) AMDGPUUseNativeCallsPass on _Z10matrix_mulPdS_S_
BISECT: running pass (243) AMDGPUSimplifyLibCallsPass on _Z10matrix_mulPdS_S_
BISECT: running pass (244) JumpThreadingPass on _Z10matrix_mulPdS_S_
BISECT: running pass (245) LoopSimplifyPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (246) LCSSAPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (247) GVNPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (248) MemCpyOptPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (249) DSEPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (250) MoveAutoInitPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (251) MergedLoadStoreMotionPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (252) LoopSimplifyPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (253) LCSSAPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (254) LoopDistributePass on scabbard.trace.device.trace_append$mem
BISECT: running pass (255) LoopVectorizePass on scabbard.trace.device.trace_append$mem
BISECT: running pass (256) InferAlignmentPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (257) LoopUnrollPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (258) WarnMissedTransformationsPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (259) SROAPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (260) InstCombinePass on scabbard.trace.device.trace_append$mem
BISECT: running pass (261) SimplifyCFGPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (262) SCCPPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (263) InstCombinePass on scabbard.trace.device.trace_append$mem
BISECT: running pass (264) BDCEPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (265) SLPVectorizerPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (266) VectorCombinePass on scabbard.trace.device.trace_append$mem
BISECT: running pass (267) InferAlignmentPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (268) InstCombinePass on scabbard.trace.device.trace_append$mem
BISECT: running pass (269) LoopSimplifyPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (270) LCSSAPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (271) AlignmentFromAssumptionsPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (272) AMDGPUUseNativeCallsPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (273) AMDGPUSimplifyLibCallsPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (274) JumpThreadingPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (275) LoopSimplifyPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (276) LCSSAPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (277) GVNPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (278) MemCpyOptPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (279) DSEPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (280) MoveAutoInitPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (281) MergedLoadStoreMotionPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (282) LoopSimplifyPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (283) LCSSAPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (284) LoopDistributePass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (285) LoopVectorizePass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (286) InferAlignmentPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (287) LoopUnrollPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (288) WarnMissedTransformationsPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (289) SROAPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (290) InstCombinePass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (291) SimplifyCFGPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (292) SCCPPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (293) InstCombinePass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (294) BDCEPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (295) SLPVectorizerPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (296) VectorCombinePass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (297) InferAlignmentPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (298) InstCombinePass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (299) LoopSimplifyPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (300) LCSSAPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (301) AlignmentFromAssumptionsPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (302) AMDGPUUseNativeCallsPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (303) AMDGPUSimplifyLibCallsPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (304) JumpThreadingPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (305) LoopSimplifyPass on _ZN4dim3C2Ejjj
BISECT: running pass (306) LCSSAPass on _ZN4dim3C2Ejjj
BISECT: running pass (307) GVNPass on _ZN4dim3C2Ejjj
BISECT: running pass (308) MemCpyOptPass on _ZN4dim3C2Ejjj
BISECT: running pass (309) DSEPass on _ZN4dim3C2Ejjj
BISECT: running pass (310) MoveAutoInitPass on _ZN4dim3C2Ejjj
BISECT: running pass (311) MergedLoadStoreMotionPass on _ZN4dim3C2Ejjj
BISECT: running pass (312) LoopSimplifyPass on _ZN4dim3C2Ejjj
BISECT: running pass (313) LCSSAPass on _ZN4dim3C2Ejjj
BISECT: running pass (314) LoopDistributePass on _ZN4dim3C2Ejjj
BISECT: running pass (315) LoopVectorizePass on _ZN4dim3C2Ejjj
BISECT: running pass (316) InferAlignmentPass on _ZN4dim3C2Ejjj
BISECT: running pass (317) LoopUnrollPass on _ZN4dim3C2Ejjj
BISECT: running pass (318) WarnMissedTransformationsPass on _ZN4dim3C2Ejjj
BISECT: running pass (319) SROAPass on _ZN4dim3C2Ejjj
BISECT: running pass (320) InstCombinePass on _ZN4dim3C2Ejjj
BISECT: running pass (321) SimplifyCFGPass on _ZN4dim3C2Ejjj
BISECT: running pass (322) SCCPPass on _ZN4dim3C2Ejjj
BISECT: running pass (323) InstCombinePass on _ZN4dim3C2Ejjj
BISECT: running pass (324) BDCEPass on _ZN4dim3C2Ejjj
BISECT: running pass (325) SLPVectorizerPass on _ZN4dim3C2Ejjj
BISECT: running pass (326) VectorCombinePass on _ZN4dim3C2Ejjj
BISECT: running pass (327) InferAlignmentPass on _ZN4dim3C2Ejjj
BISECT: running pass (328) InstCombinePass on _ZN4dim3C2Ejjj
BISECT: running pass (329) LoopSimplifyPass on _ZN4dim3C2Ejjj
BISECT: running pass (330) LCSSAPass on _ZN4dim3C2Ejjj
BISECT: running pass (331) AlignmentFromAssumptionsPass on _ZN4dim3C2Ejjj
BISECT: running pass (332) AMDGPUUseNativeCallsPass on _ZN4dim3C2Ejjj
BISECT: running pass (333) AMDGPUSimplifyLibCallsPass on _ZN4dim3C2Ejjj
BISECT: running pass (334) JumpThreadingPass on _ZN4dim3C2Ejjj
BISECT: running pass (335) LowerTypeTestsPass on [module]
BISECT: running pass (336) LowerTypeTestsPass on [module]
BISECT: running pass (337) LoopSinkPass on __cxa_pure_virtual
BISECT: running pass (338) DivRemPairsPass on __cxa_pure_virtual
BISECT: running pass (339) SimplifyCFGPass on __cxa_pure_virtual
BISECT: running pass (340) LoopSinkPass on __cxa_deleted_virtual
BISECT: running pass (341) DivRemPairsPass on __cxa_deleted_virtual
BISECT: running pass (342) SimplifyCFGPass on __cxa_deleted_virtual
BISECT: running pass (343) LoopSinkPass on _Z10matrix_mulPdS_S_
BISECT: running pass (344) DivRemPairsPass on _Z10matrix_mulPdS_S_
BISECT: running pass (345) SimplifyCFGPass on _Z10matrix_mulPdS_S_
BISECT: running pass (346) LoopSinkPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (347) DivRemPairsPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (348) SimplifyCFGPass on scabbard.trace.device.trace_append$mem
BISECT: running pass (349) LoopSinkPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (350) DivRemPairsPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (351) SimplifyCFGPass on scabbard.trace.device.trace_append$alloc
BISECT: running pass (352) LoopSinkPass on _ZN4dim3C2Ejjj
BISECT: running pass (353) DivRemPairsPass on _ZN4dim3C2Ejjj
BISECT: running pass (354) SimplifyCFGPass on _ZN4dim3C2Ejjj
BISECT: running pass (355) EliminateAvailableExternallyPass on [module]
BISECT: running pass (356) GlobalDCEPass on [module]
BISECT: running pass (357) CGProfilePass on [module]
BISECT: running pass (358) AMDGPULowerModuleLDSPass on [module]

[scabbard.instr.device.run:DBG] running instrumentation pass on device/GPU module (LTO)
BISECT: running pass (359) AMDGPU Image Intrinsic Optimizer on function (__cxa_pure_virtual)
BISECT: running pass (360) AMDGPU Image Intrinsic Optimizer on function (__cxa_deleted_virtual)
BISECT: running pass (361) AMDGPU Image Intrinsic Optimizer on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (362) AMDGPU Image Intrinsic Optimizer on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (363) AMDGPU Image Intrinsic Optimizer on function (_ZN4dim3C2Ejjj)
BISECT: running pass (364) AMDGPU Image Intrinsic Optimizer on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (365) Infer address spaces on function (__cxa_pure_virtual)
BISECT: running pass (366) AMDGPU atomic optimizations on function (__cxa_pure_virtual)
BISECT: running pass (367) AMDGPU Promote Alloca on function (__cxa_pure_virtual)
BISECT: running pass (368) Split GEPs to a variadic base and a constant offset for better CSE on function (__cxa_pure_virtual)
BISECT: running pass (369) Straight line strength reduction on function (__cxa_pure_virtual)
BISECT: running pass (370) Global Value Numbering on function (__cxa_pure_virtual)
BISECT: running pass (371) Nary reassociation on function (__cxa_pure_virtual)
BISECT: running pass (372) Early CSE on function (__cxa_pure_virtual)
BISECT: running pass (373) AMDGPU IR optimizations on function (__cxa_pure_virtual)
BISECT: running pass (374) Merge contiguous icmps into a memcmp on function (__cxa_pure_virtual)
BISECT: running pass (375) Expand memcmp() to load/stores on function (__cxa_pure_virtual)
BISECT: running pass (376) Constant Hoisting on function (__cxa_pure_virtual)
BISECT: running pass (377) Partially inline calls to library functions on function (__cxa_pure_virtual)
BISECT: running pass (378) TLS Variable Hoist on function (__cxa_pure_virtual)
BISECT: running pass (379) Global Value Numbering on function (__cxa_pure_virtual)
BISECT: running pass (380) Infer address spaces on function (__cxa_deleted_virtual)
BISECT: running pass (381) AMDGPU atomic optimizations on function (__cxa_deleted_virtual)
BISECT: running pass (382) AMDGPU Promote Alloca on function (__cxa_deleted_virtual)
BISECT: running pass (383) Split GEPs to a variadic base and a constant offset for better CSE on function (__cxa_deleted_virtual)
BISECT: running pass (384) Straight line strength reduction on function (__cxa_deleted_virtual)
BISECT: running pass (385) Global Value Numbering on function (__cxa_deleted_virtual)
BISECT: running pass (386) Nary reassociation on function (__cxa_deleted_virtual)
BISECT: running pass (387) Early CSE on function (__cxa_deleted_virtual)
BISECT: running pass (388) AMDGPU IR optimizations on function (__cxa_deleted_virtual)
BISECT: running pass (389) Merge contiguous icmps into a memcmp on function (__cxa_deleted_virtual)
BISECT: running pass (390) Expand memcmp() to load/stores on function (__cxa_deleted_virtual)
BISECT: running pass (391) Constant Hoisting on function (__cxa_deleted_virtual)
BISECT: running pass (392) Partially inline calls to library functions on function (__cxa_deleted_virtual)
BISECT: running pass (393) TLS Variable Hoist on function (__cxa_deleted_virtual)
BISECT: running pass (394) Global Value Numbering on function (__cxa_deleted_virtual)
BISECT: running pass (395) Infer address spaces on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (396) AMDGPU atomic optimizations on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (397) AMDGPU Promote Alloca on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (398) Split GEPs to a variadic base and a constant offset for better CSE on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (399) Straight line strength reduction on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (400) Global Value Numbering on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (401) Nary reassociation on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (402) Early CSE on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (403) AMDGPU IR optimizations on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (404) Merge contiguous icmps into a memcmp on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (405) Expand memcmp() to load/stores on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (406) Constant Hoisting on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (407) Partially inline calls to library functions on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (408) TLS Variable Hoist on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (409) Global Value Numbering on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (410) Infer address spaces on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (411) AMDGPU atomic optimizations on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (412) AMDGPU Promote Alloca on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (413) Split GEPs to a variadic base and a constant offset for better CSE on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (414) Straight line strength reduction on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (415) Global Value Numbering on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (416) Nary reassociation on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (417) Early CSE on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (418) AMDGPU IR optimizations on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (419) Merge contiguous icmps into a memcmp on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (420) Expand memcmp() to load/stores on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (421) Constant Hoisting on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (422) Partially inline calls to library functions on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (423) TLS Variable Hoist on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (424) Global Value Numbering on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (425) Infer address spaces on function (_ZN4dim3C2Ejjj)
BISECT: running pass (426) AMDGPU atomic optimizations on function (_ZN4dim3C2Ejjj)
BISECT: running pass (427) AMDGPU Promote Alloca on function (_ZN4dim3C2Ejjj)
BISECT: running pass (428) Split GEPs to a variadic base and a constant offset for better CSE on function (_ZN4dim3C2Ejjj)
BISECT: running pass (429) Straight line strength reduction on function (_ZN4dim3C2Ejjj)
BISECT: running pass (430) Global Value Numbering on function (_ZN4dim3C2Ejjj)
BISECT: running pass (431) Nary reassociation on function (_ZN4dim3C2Ejjj)
BISECT: running pass (432) Early CSE on function (_ZN4dim3C2Ejjj)
BISECT: running pass (433) AMDGPU IR optimizations on function (_ZN4dim3C2Ejjj)
BISECT: running pass (434) Merge contiguous icmps into a memcmp on function (_ZN4dim3C2Ejjj)
BISECT: running pass (435) Expand memcmp() to load/stores on function (_ZN4dim3C2Ejjj)
BISECT: running pass (436) Constant Hoisting on function (_ZN4dim3C2Ejjj)
BISECT: running pass (437) Partially inline calls to library functions on function (_ZN4dim3C2Ejjj)
BISECT: running pass (438) TLS Variable Hoist on function (_ZN4dim3C2Ejjj)
BISECT: running pass (439) Global Value Numbering on function (_ZN4dim3C2Ejjj)
BISECT: running pass (440) Infer address spaces on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (441) AMDGPU atomic optimizations on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (442) AMDGPU Promote Alloca on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (443) Split GEPs to a variadic base and a constant offset for better CSE on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (444) Straight line strength reduction on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (445) Global Value Numbering on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (446) Nary reassociation on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (447) Early CSE on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (448) AMDGPU IR optimizations on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (449) Merge contiguous icmps into a memcmp on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (450) Expand memcmp() to load/stores on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (451) Constant Hoisting on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (452) Partially inline calls to library functions on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (453) TLS Variable Hoist on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (454) Global Value Numbering on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (455) CodeGen Prepare on function (__cxa_pure_virtual)
BISECT: running pass (456) GPU Load and Store Vectorizer on function (__cxa_pure_virtual)
BISECT: running pass (457) AMDGPU IR late optimizations on function (__cxa_pure_virtual)
BISECT: running pass (458) AMDGPU Annotate Uniform Values on function (__cxa_pure_virtual)
BISECT: running pass (459) AMDGPU DAG->DAG Pattern Instruction Selection on function (__cxa_pure_virtual)
BISECT: running pass (460) Early Tail Duplication on function (__cxa_pure_virtual)
BISECT: running pass (461) Optimize machine instruction PHIs on function (__cxa_pure_virtual)
BISECT: running pass (462) Remove dead machine instructions on function (__cxa_pure_virtual)
BISECT: running pass (463) Early Machine Loop Invariant Code Motion on function (__cxa_pure_virtual)
BISECT: running pass (464) Machine Common Subexpression Elimination on function (__cxa_pure_virtual)
BISECT: running pass (465) Machine code sinking on function (__cxa_pure_virtual)
BISECT: running pass (466) Peephole Optimizations on function (__cxa_pure_virtual)
BISECT: running pass (467) Remove dead machine instructions on function (__cxa_pure_virtual)
BISECT: running pass (468) SI Fold Operands on function (__cxa_pure_virtual)
BISECT: running pass (469) GCN DPP Combine on function (__cxa_pure_virtual)
BISECT: running pass (470) SI Load Store Optimizer on function (__cxa_pure_virtual)
BISECT: running pass (471) SI Peephole SDWA on function (__cxa_pure_virtual)
BISECT: running pass (472) Early Machine Loop Invariant Code Motion on function (__cxa_pure_virtual)
BISECT: running pass (473) Machine Common Subexpression Elimination on function (__cxa_pure_virtual)
BISECT: running pass (474) SI Fold Operands on function (__cxa_pure_virtual)
BISECT: running pass (475) Remove dead machine instructions on function (__cxa_pure_virtual)
BISECT: running pass (476) SI Shrink Instructions on function (__cxa_pure_virtual)
BISECT: running pass (477) Remove dead machine instructions on function (__cxa_pure_virtual)
BISECT: running pass (478) SI Optimize VGPR LiveRange on function (__cxa_pure_virtual)
BISECT: running pass (479) Two-Address instruction pass on function (__cxa_pure_virtual)
BISECT: running pass (480) AMDGPU Pre-RA optimizations on function (__cxa_pure_virtual)
BISECT: running pass (481) Machine Instruction Scheduler on function (__cxa_pure_virtual)
BISECT: running pass (482) SI optimize exec mask operations pre-RA on function (__cxa_pure_virtual)
BISECT: running pass (483) SI Form memory clauses on function (__cxa_pure_virtual)
BISECT: running pass (484) AMDGPU Mark Last Scratch Load on function (__cxa_pure_virtual)
BISECT: running pass (485) Stack Slot Coloring on function (__cxa_pure_virtual)
BISECT: running pass (486) Machine Copy Propagation Pass on function (__cxa_pure_virtual)
BISECT: running pass (487) Machine Loop Invariant Code Motion on function (__cxa_pure_virtual)
BISECT: running pass (488) SI optimize exec mask operations on function (__cxa_pure_virtual)
BISECT: running pass (489) Fixup Statepoint Caller Saved on function (__cxa_pure_virtual)
BISECT: running pass (490) PostRA Machine Sink on function (__cxa_pure_virtual)
BISECT: running pass (491) Shrink Wrapping analysis on function (__cxa_pure_virtual)
BISECT: running pass (492) Machine Late Instructions Cleanup Pass on function (__cxa_pure_virtual)
BISECT: running pass (493) Control Flow Optimizer on function (__cxa_pure_virtual)
BISECT: running pass (494) Tail Duplication on function (__cxa_pure_virtual)
BISECT: running pass (495) Machine Copy Propagation Pass on function (__cxa_pure_virtual)
BISECT: running pass (496) SI Shrink Instructions on function (__cxa_pure_virtual)
BISECT: running pass (497) SI post-RA bundler on function (__cxa_pure_virtual)
BISECT: running pass (498) PostRA Machine Instruction Scheduler on function (__cxa_pure_virtual)
BISECT: running pass (499) Branch Probability Basic Block Placement on function (__cxa_pure_virtual)
BISECT: running pass (500) GCN Create VOPD Instructions on function (__cxa_pure_virtual)
BISECT: running pass (501) SI Insert Hard Clauses on function (__cxa_pure_virtual)
BISECT: running pass (502) AMDGPU Insert Delay ALU on function (__cxa_pure_virtual)
BISECT: running pass (503) CodeGen Prepare on function (__cxa_deleted_virtual)
BISECT: running pass (504) GPU Load and Store Vectorizer on function (__cxa_deleted_virtual)
BISECT: running pass (505) AMDGPU IR late optimizations on function (__cxa_deleted_virtual)
BISECT: running pass (506) AMDGPU Annotate Uniform Values on function (__cxa_deleted_virtual)
BISECT: running pass (507) AMDGPU DAG->DAG Pattern Instruction Selection on function (__cxa_deleted_virtual)
BISECT: running pass (508) Early Tail Duplication on function (__cxa_deleted_virtual)
BISECT: running pass (509) Optimize machine instruction PHIs on function (__cxa_deleted_virtual)
BISECT: running pass (510) Remove dead machine instructions on function (__cxa_deleted_virtual)
BISECT: running pass (511) Early Machine Loop Invariant Code Motion on function (__cxa_deleted_virtual)
BISECT: running pass (512) Machine Common Subexpression Elimination on function (__cxa_deleted_virtual)
BISECT: running pass (513) Machine code sinking on function (__cxa_deleted_virtual)
BISECT: running pass (514) Peephole Optimizations on function (__cxa_deleted_virtual)
BISECT: running pass (515) Remove dead machine instructions on function (__cxa_deleted_virtual)
BISECT: running pass (516) SI Fold Operands on function (__cxa_deleted_virtual)
BISECT: running pass (517) GCN DPP Combine on function (__cxa_deleted_virtual)
BISECT: running pass (518) SI Load Store Optimizer on function (__cxa_deleted_virtual)
BISECT: running pass (519) SI Peephole SDWA on function (__cxa_deleted_virtual)
BISECT: running pass (520) Early Machine Loop Invariant Code Motion on function (__cxa_deleted_virtual)
BISECT: running pass (521) Machine Common Subexpression Elimination on function (__cxa_deleted_virtual)
BISECT: running pass (522) SI Fold Operands on function (__cxa_deleted_virtual)
BISECT: running pass (523) Remove dead machine instructions on function (__cxa_deleted_virtual)
BISECT: running pass (524) SI Shrink Instructions on function (__cxa_deleted_virtual)
BISECT: running pass (525) Remove dead machine instructions on function (__cxa_deleted_virtual)
BISECT: running pass (526) SI Optimize VGPR LiveRange on function (__cxa_deleted_virtual)
BISECT: running pass (527) Two-Address instruction pass on function (__cxa_deleted_virtual)
BISECT: running pass (528) AMDGPU Pre-RA optimizations on function (__cxa_deleted_virtual)
BISECT: running pass (529) Machine Instruction Scheduler on function (__cxa_deleted_virtual)
BISECT: running pass (530) SI optimize exec mask operations pre-RA on function (__cxa_deleted_virtual)
BISECT: running pass (531) SI Form memory clauses on function (__cxa_deleted_virtual)
BISECT: running pass (532) AMDGPU Mark Last Scratch Load on function (__cxa_deleted_virtual)
BISECT: running pass (533) Stack Slot Coloring on function (__cxa_deleted_virtual)
BISECT: running pass (534) Machine Copy Propagation Pass on function (__cxa_deleted_virtual)
BISECT: running pass (535) Machine Loop Invariant Code Motion on function (__cxa_deleted_virtual)
BISECT: running pass (536) SI optimize exec mask operations on function (__cxa_deleted_virtual)
BISECT: running pass (537) Fixup Statepoint Caller Saved on function (__cxa_deleted_virtual)
BISECT: running pass (538) PostRA Machine Sink on function (__cxa_deleted_virtual)
BISECT: running pass (539) Shrink Wrapping analysis on function (__cxa_deleted_virtual)
BISECT: running pass (540) Machine Late Instructions Cleanup Pass on function (__cxa_deleted_virtual)
BISECT: running pass (541) Control Flow Optimizer on function (__cxa_deleted_virtual)
BISECT: running pass (542) Tail Duplication on function (__cxa_deleted_virtual)
BISECT: running pass (543) Machine Copy Propagation Pass on function (__cxa_deleted_virtual)
BISECT: running pass (544) SI Shrink Instructions on function (__cxa_deleted_virtual)
BISECT: running pass (545) SI post-RA bundler on function (__cxa_deleted_virtual)
BISECT: running pass (546) PostRA Machine Instruction Scheduler on function (__cxa_deleted_virtual)
BISECT: running pass (547) Branch Probability Basic Block Placement on function (__cxa_deleted_virtual)
BISECT: running pass (548) GCN Create VOPD Instructions on function (__cxa_deleted_virtual)
BISECT: running pass (549) SI Insert Hard Clauses on function (__cxa_deleted_virtual)
BISECT: running pass (550) AMDGPU Insert Delay ALU on function (__cxa_deleted_virtual)
BISECT: running pass (551) CodeGen Prepare on function (_ZN4dim3C2Ejjj)
BISECT: running pass (552) GPU Load and Store Vectorizer on function (_ZN4dim3C2Ejjj)
BISECT: running pass (553) AMDGPU IR late optimizations on function (_ZN4dim3C2Ejjj)
BISECT: running pass (554) AMDGPU Annotate Uniform Values on function (_ZN4dim3C2Ejjj)
BISECT: running pass (555) AMDGPU DAG->DAG Pattern Instruction Selection on function (_ZN4dim3C2Ejjj)
BISECT: running pass (556) Early Tail Duplication on function (_ZN4dim3C2Ejjj)
BISECT: running pass (557) Optimize machine instruction PHIs on function (_ZN4dim3C2Ejjj)
BISECT: running pass (558) Remove dead machine instructions on function (_ZN4dim3C2Ejjj)
BISECT: running pass (559) Early Machine Loop Invariant Code Motion on function (_ZN4dim3C2Ejjj)
BISECT: running pass (560) Machine Common Subexpression Elimination on function (_ZN4dim3C2Ejjj)
BISECT: running pass (561) Machine code sinking on function (_ZN4dim3C2Ejjj)
BISECT: running pass (562) Peephole Optimizations on function (_ZN4dim3C2Ejjj)
BISECT: running pass (563) Remove dead machine instructions on function (_ZN4dim3C2Ejjj)
BISECT: running pass (564) SI Fold Operands on function (_ZN4dim3C2Ejjj)
BISECT: running pass (565) GCN DPP Combine on function (_ZN4dim3C2Ejjj)
BISECT: running pass (566) SI Load Store Optimizer on function (_ZN4dim3C2Ejjj)
BISECT: running pass (567) SI Peephole SDWA on function (_ZN4dim3C2Ejjj)
BISECT: running pass (568) Early Machine Loop Invariant Code Motion on function (_ZN4dim3C2Ejjj)
BISECT: running pass (569) Machine Common Subexpression Elimination on function (_ZN4dim3C2Ejjj)
BISECT: running pass (570) SI Fold Operands on function (_ZN4dim3C2Ejjj)
BISECT: running pass (571) Remove dead machine instructions on function (_ZN4dim3C2Ejjj)
BISECT: running pass (572) SI Shrink Instructions on function (_ZN4dim3C2Ejjj)
BISECT: running pass (573) Remove dead machine instructions on function (_ZN4dim3C2Ejjj)
BISECT: running pass (574) SI Optimize VGPR LiveRange on function (_ZN4dim3C2Ejjj)
BISECT: running pass (575) Two-Address instruction pass on function (_ZN4dim3C2Ejjj)
BISECT: running pass (576) AMDGPU Pre-RA optimizations on function (_ZN4dim3C2Ejjj)
BISECT: running pass (577) Machine Instruction Scheduler on function (_ZN4dim3C2Ejjj)
BISECT: running pass (578) SI optimize exec mask operations pre-RA on function (_ZN4dim3C2Ejjj)
BISECT: running pass (579) SI Form memory clauses on function (_ZN4dim3C2Ejjj)
BISECT: running pass (580) AMDGPU Mark Last Scratch Load on function (_ZN4dim3C2Ejjj)
BISECT: running pass (581) Stack Slot Coloring on function (_ZN4dim3C2Ejjj)
BISECT: running pass (582) Machine Copy Propagation Pass on function (_ZN4dim3C2Ejjj)
BISECT: running pass (583) Machine Loop Invariant Code Motion on function (_ZN4dim3C2Ejjj)
BISECT: running pass (584) SI optimize exec mask operations on function (_ZN4dim3C2Ejjj)
BISECT: running pass (585) Fixup Statepoint Caller Saved on function (_ZN4dim3C2Ejjj)
BISECT: running pass (586) PostRA Machine Sink on function (_ZN4dim3C2Ejjj)
BISECT: running pass (587) Shrink Wrapping analysis on function (_ZN4dim3C2Ejjj)
BISECT: running pass (588) Machine Late Instructions Cleanup Pass on function (_ZN4dim3C2Ejjj)
BISECT: running pass (589) Control Flow Optimizer on function (_ZN4dim3C2Ejjj)
BISECT: running pass (590) Tail Duplication on function (_ZN4dim3C2Ejjj)
BISECT: running pass (591) Machine Copy Propagation Pass on function (_ZN4dim3C2Ejjj)
BISECT: running pass (592) SI Shrink Instructions on function (_ZN4dim3C2Ejjj)
BISECT: running pass (593) SI post-RA bundler on function (_ZN4dim3C2Ejjj)
BISECT: running pass (594) PostRA Machine Instruction Scheduler on function (_ZN4dim3C2Ejjj)
BISECT: running pass (595) Branch Probability Basic Block Placement on function (_ZN4dim3C2Ejjj)
BISECT: running pass (596) GCN Create VOPD Instructions on function (_ZN4dim3C2Ejjj)
BISECT: running pass (597) SI Insert Hard Clauses on function (_ZN4dim3C2Ejjj)
BISECT: running pass (598) AMDGPU Insert Delay ALU on function (_ZN4dim3C2Ejjj)
BISECT: running pass (599) CodeGen Prepare on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (600) GPU Load and Store Vectorizer on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (601) AMDGPU IR late optimizations on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (602) AMDGPU Annotate Uniform Values on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (603) AMDGPU DAG->DAG Pattern Instruction Selection on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (604) Early Tail Duplication on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (605) Optimize machine instruction PHIs on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (606) Remove dead machine instructions on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (607) Early Machine Loop Invariant Code Motion on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (608) Machine Common Subexpression Elimination on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (609) Machine code sinking on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (610) Peephole Optimizations on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (611) Remove dead machine instructions on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (612) SI Fold Operands on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (613) GCN DPP Combine on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (614) SI Load Store Optimizer on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (615) SI Peephole SDWA on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (616) Early Machine Loop Invariant Code Motion on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (617) Machine Common Subexpression Elimination on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (618) SI Fold Operands on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (619) Remove dead machine instructions on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (620) SI Shrink Instructions on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (621) Remove dead machine instructions on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (622) SI Optimize VGPR LiveRange on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (623) Two-Address instruction pass on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (624) AMDGPU Pre-RA optimizations on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (625) Machine Instruction Scheduler on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (626) SI optimize exec mask operations pre-RA on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (627) SI Form memory clauses on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (628) AMDGPU Mark Last Scratch Load on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (629) Stack Slot Coloring on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (630) Machine Copy Propagation Pass on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (631) Machine Loop Invariant Code Motion on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (632) SI optimize exec mask operations on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (633) Fixup Statepoint Caller Saved on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (634) PostRA Machine Sink on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (635) Shrink Wrapping analysis on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (636) Machine Late Instructions Cleanup Pass on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (637) Control Flow Optimizer on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (638) Tail Duplication on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (639) Machine Copy Propagation Pass on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (640) SI Shrink Instructions on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (641) SI post-RA bundler on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (642) PostRA Machine Instruction Scheduler on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (643) Branch Probability Basic Block Placement on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (644) GCN Create VOPD Instructions on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (645) SI Insert Hard Clauses on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (646) AMDGPU Insert Delay ALU on function (scabbard.trace.device.trace_append$mem)
BISECT: running pass (647) CodeGen Prepare on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (648) GPU Load and Store Vectorizer on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (649) AMDGPU IR late optimizations on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (650) AMDGPU Annotate Uniform Values on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (651) AMDGPU DAG->DAG Pattern Instruction Selection on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (652) Early Tail Duplication on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (653) Optimize machine instruction PHIs on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (654) Remove dead machine instructions on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (655) Early Machine Loop Invariant Code Motion on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (656) Machine Common Subexpression Elimination on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (657) Machine code sinking on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (658) Peephole Optimizations on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (659) Remove dead machine instructions on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (660) SI Fold Operands on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (661) GCN DPP Combine on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (662) SI Load Store Optimizer on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (663) SI Peephole SDWA on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (664) Early Machine Loop Invariant Code Motion on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (665) Machine Common Subexpression Elimination on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (666) SI Fold Operands on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (667) Remove dead machine instructions on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (668) SI Shrink Instructions on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (669) Remove dead machine instructions on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (670) SI Optimize VGPR LiveRange on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (671) Two-Address instruction pass on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (672) AMDGPU Pre-RA optimizations on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (673) Machine Instruction Scheduler on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (674) SI optimize exec mask operations pre-RA on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (675) SI Form memory clauses on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (676) AMDGPU Mark Last Scratch Load on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (677) Stack Slot Coloring on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (678) Machine Copy Propagation Pass on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (679) Machine Loop Invariant Code Motion on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (680) SI optimize exec mask operations on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (681) Fixup Statepoint Caller Saved on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (682) PostRA Machine Sink on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (683) Shrink Wrapping analysis on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (684) Machine Late Instructions Cleanup Pass on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (685) Control Flow Optimizer on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (686) Tail Duplication on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (687) Machine Copy Propagation Pass on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (688) SI Shrink Instructions on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (689) SI post-RA bundler on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (690) PostRA Machine Instruction Scheduler on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (691) Branch Probability Basic Block Placement on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (692) GCN Create VOPD Instructions on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (693) SI Insert Hard Clauses on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (694) AMDGPU Insert Delay ALU on function (scabbard.trace.device.trace_append$alloc)
BISECT: running pass (695) CodeGen Prepare on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (696) GPU Load and Store Vectorizer on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (697) AMDGPU IR late optimizations on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (698) AMDGPU Annotate Uniform Values on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (699) AMDGPU DAG->DAG Pattern Instruction Selection on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (700) Early Tail Duplication on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (701) Optimize machine instruction PHIs on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (702) Merge disjoint stack slots on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (703) Remove dead machine instructions on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (704) Early Machine Loop Invariant Code Motion on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (705) Machine Common Subexpression Elimination on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (706) Machine code sinking on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (707) Peephole Optimizations on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (708) Remove dead machine instructions on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (709) SI Fold Operands on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (710) GCN DPP Combine on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (711) SI Load Store Optimizer on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (712) SI Peephole SDWA on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (713) Early Machine Loop Invariant Code Motion on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (714) Machine Common Subexpression Elimination on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (715) SI Fold Operands on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (716) Remove dead machine instructions on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (717) SI Shrink Instructions on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (718) Remove dead machine instructions on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (719) SI Optimize VGPR LiveRange on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (720) Two-Address instruction pass on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (721) AMDGPU Pre-RA optimizations on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (722) Machine Instruction Scheduler on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (723) SI optimize exec mask operations pre-RA on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (724) SI Form memory clauses on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (725) AMDGPU Mark Last Scratch Load on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (726) Stack Slot Coloring on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (727) Machine Copy Propagation Pass on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (728) Machine Loop Invariant Code Motion on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (729) SI optimize exec mask operations on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (730) Fixup Statepoint Caller Saved on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (731) PostRA Machine Sink on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (732) Shrink Wrapping analysis on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (733) Machine Late Instructions Cleanup Pass on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (734) Control Flow Optimizer on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (735) Tail Duplication on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (736) Machine Copy Propagation Pass on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (737) SI Shrink Instructions on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (738) SI post-RA bundler on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (739) PostRA Machine Instruction Scheduler on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (740) Branch Probability Basic Block Placement on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (741) GCN Create VOPD Instructions on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (742) SI Insert Hard Clauses on function (_Z10matrix_mulPdS_S_)
BISECT: running pass (743) AMDGPU Insert Delay ALU on function (_Z10matrix_mulPdS_S_)
```

LTO passes for Host
```
[scabbard.instr.device.run:DBG] running instrumentation pass on device/GPU module (LTO)
BISECT: running pass (1) CrossDSOCFIPass on [module]
BISECT: running pass (2) OpenMPOptPass on [module]
BISECT: running pass (3) GlobalDCEPass on [module]
BISECT: running pass (4) InferFunctionAttrsPass on [module]
BISECT: running pass (5) CallSiteSplittingPass on __hip_module_ctor
BISECT: running pass (6) CallSiteSplittingPass on __hip_module_dtor
BISECT: running pass (7) CallSiteSplittingPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (8) CallSiteSplittingPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (9) CallSiteSplittingPass on main
BISECT: running pass (10) PGOIndirectCallPromotion on [module]
BISECT: running pass (11) IPSCCPPass on [module]
BISECT: running pass (12) CalledValuePropagationPass on [module]
BISECT: running pass (13) PostOrderFunctionAttrsPass on (_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_)
BISECT: running pass (14) PostOrderFunctionAttrsPass on (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (15) PostOrderFunctionAttrsPass on (main)
BISECT: running pass (16) PostOrderFunctionAttrsPass on (__hip_module_dtor)
BISECT: running pass (17) PostOrderFunctionAttrsPass on (__hip_module_ctor)
BISECT: running pass (18) ReversePostOrderFunctionAttrsPass on [module]
BISECT: running pass (19) GlobalSplitPass on [module]
BISECT: running pass (20) WholeProgramDevirtPass on [module]
BISECT: running pass (21) GlobalOptPass on [module]
BISECT: running pass (22) PromotePass on __hip_module_ctor
BISECT: running pass (23) PromotePass on __hip_module_dtor
BISECT: running pass (24) PromotePass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (25) PromotePass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (26) PromotePass on main
BISECT: running pass (27) ConstantMergePass on [module]
BISECT: running pass (28) DeadArgumentEliminationPass on [module]
BISECT: running pass (29) InstCombinePass on __hip_module_ctor
BISECT: running pass (30) AggressiveInstCombinePass on __hip_module_ctor
BISECT: running pass (31) InstCombinePass on __hip_module_dtor
BISECT: running pass (32) AggressiveInstCombinePass on __hip_module_dtor
BISECT: running pass (33) InstCombinePass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (34) AggressiveInstCombinePass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (35) InstCombinePass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (36) AggressiveInstCombinePass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (37) InstCombinePass on main
BISECT: running pass (38) AggressiveInstCombinePass on main
BISECT: running pass (39) InlinerPass on (_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_)
BISECT: running pass (40) InlinerPass on (_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_)
BISECT: running pass (41) InlinerPass on (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (42) InlinerPass on (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (43) InlinerPass on (main)
BISECT: running pass (44) InlinerPass on (main)
BISECT: running pass (45) InlinerPass on (__hip_module_dtor)
BISECT: running pass (46) InlinerPass on (__hip_module_dtor)
BISECT: running pass (47) InlinerPass on (__hip_module_ctor)
BISECT: running pass (48) InlinerPass on (__hip_module_ctor)
BISECT: running pass (49) GlobalOptPass on [module]
BISECT: running pass (50) OpenMPOptPass on [module]
BISECT: running pass (51) GlobalDCEPass on [module]
BISECT: running pass (52) ArgumentPromotionPass on (_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_)
BISECT: running pass (53) ArgumentPromotionPass on (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (54) ArgumentPromotionPass on (main)
BISECT: running pass (55) ArgumentPromotionPass on (__hip_module_dtor)
BISECT: running pass (56) ArgumentPromotionPass on (__hip_module_ctor)
BISECT: running pass (57) InstCombinePass on __hip_module_ctor
BISECT: running pass (58) ConstraintEliminationPass on __hip_module_ctor
BISECT: running pass (59) JumpThreadingPass on __hip_module_ctor
BISECT: running pass (60) SROAPass on __hip_module_ctor
BISECT: running pass (61) TailCallElimPass on __hip_module_ctor
BISECT: running pass (62) InstCombinePass on __hip_module_dtor
BISECT: running pass (63) ConstraintEliminationPass on __hip_module_dtor
BISECT: running pass (64) JumpThreadingPass on __hip_module_dtor
BISECT: running pass (65) SROAPass on __hip_module_dtor
BISECT: running pass (66) TailCallElimPass on __hip_module_dtor
BISECT: running pass (67) InstCombinePass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (68) ConstraintEliminationPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (69) JumpThreadingPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (70) SROAPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (71) TailCallElimPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (72) InstCombinePass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (73) ConstraintEliminationPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (74) JumpThreadingPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (75) SROAPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (76) TailCallElimPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (77) InstCombinePass on main
BISECT: running pass (78) ConstraintEliminationPass on main
BISECT: running pass (79) JumpThreadingPass on main
BISECT: running pass (80) SROAPass on main
BISECT: running pass (81) TailCallElimPass on main
BISECT: running pass (82) PostOrderFunctionAttrsPass on (_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_)
BISECT: running pass (83) PostOrderFunctionAttrsPass on (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (84) PostOrderFunctionAttrsPass on (main)
BISECT: running pass (85) PostOrderFunctionAttrsPass on (__hip_module_dtor)
BISECT: running pass (86) PostOrderFunctionAttrsPass on (__hip_module_ctor)
BISECT: running pass (87) InvalidateAnalysisPass<llvm::AAManager> on __hip_module_ctor
BISECT: running pass (88) InvalidateAnalysisPass<llvm::AAManager> on __hip_module_dtor
BISECT: running pass (89) InvalidateAnalysisPass<llvm::AAManager> on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (90) InvalidateAnalysisPass<llvm::AAManager> on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (91) InvalidateAnalysisPass<llvm::AAManager> on main
BISECT: running pass (92) OpenMPOptCGSCCPass on (_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_)
BISECT: running pass (93) OpenMPOptCGSCCPass on (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (94) OpenMPOptCGSCCPass on (main)
BISECT: running pass (95) OpenMPOptCGSCCPass on (__hip_module_dtor)
BISECT: running pass (96) OpenMPOptCGSCCPass on (__hip_module_ctor)
BISECT: running pass (97) LoopSimplifyPass on __hip_module_ctor
BISECT: running pass (98) LCSSAPass on __hip_module_ctor
BISECT: running pass (99) GVNPass on __hip_module_ctor
BISECT: running pass (100) MemCpyOptPass on __hip_module_ctor
BISECT: running pass (101) DSEPass on __hip_module_ctor
BISECT: running pass (102) MoveAutoInitPass on __hip_module_ctor
BISECT: running pass (103) MergedLoadStoreMotionPass on __hip_module_ctor
BISECT: running pass (104) LoopSimplifyPass on __hip_module_ctor
BISECT: running pass (105) LCSSAPass on __hip_module_ctor
BISECT: running pass (106) LoopDistributePass on __hip_module_ctor
BISECT: running pass (107) LoopVectorizePass on __hip_module_ctor
BISECT: running pass (108) InferAlignmentPass on __hip_module_ctor
BISECT: running pass (109) LoopUnrollPass on __hip_module_ctor
BISECT: running pass (110) WarnMissedTransformationsPass on __hip_module_ctor
BISECT: running pass (111) SROAPass on __hip_module_ctor
BISECT: running pass (112) InstCombinePass on __hip_module_ctor
BISECT: running pass (113) SimplifyCFGPass on __hip_module_ctor
BISECT: running pass (114) SCCPPass on __hip_module_ctor
BISECT: running pass (115) InstCombinePass on __hip_module_ctor
BISECT: running pass (116) BDCEPass on __hip_module_ctor
BISECT: running pass (117) SLPVectorizerPass on __hip_module_ctor
BISECT: running pass (118) VectorCombinePass on __hip_module_ctor
BISECT: running pass (119) InferAlignmentPass on __hip_module_ctor
BISECT: running pass (120) InstCombinePass on __hip_module_ctor
BISECT: running pass (121) LoopSimplifyPass on __hip_module_ctor
BISECT: running pass (122) LCSSAPass on __hip_module_ctor
BISECT: running pass (123) AlignmentFromAssumptionsPass on __hip_module_ctor
BISECT: running pass (124) JumpThreadingPass on __hip_module_ctor
BISECT: running pass (125) LoopSimplifyPass on __hip_module_dtor
BISECT: running pass (126) LCSSAPass on __hip_module_dtor
BISECT: running pass (127) GVNPass on __hip_module_dtor
BISECT: running pass (128) MemCpyOptPass on __hip_module_dtor
BISECT: running pass (129) DSEPass on __hip_module_dtor
BISECT: running pass (130) MoveAutoInitPass on __hip_module_dtor
BISECT: running pass (131) MergedLoadStoreMotionPass on __hip_module_dtor
BISECT: running pass (132) LoopSimplifyPass on __hip_module_dtor
BISECT: running pass (133) LCSSAPass on __hip_module_dtor
BISECT: running pass (134) LoopDistributePass on __hip_module_dtor
BISECT: running pass (135) LoopVectorizePass on __hip_module_dtor
BISECT: running pass (136) InferAlignmentPass on __hip_module_dtor
BISECT: running pass (137) LoopUnrollPass on __hip_module_dtor
BISECT: running pass (138) WarnMissedTransformationsPass on __hip_module_dtor
BISECT: running pass (139) SROAPass on __hip_module_dtor
BISECT: running pass (140) InstCombinePass on __hip_module_dtor
BISECT: running pass (141) SimplifyCFGPass on __hip_module_dtor
BISECT: running pass (142) SCCPPass on __hip_module_dtor
BISECT: running pass (143) InstCombinePass on __hip_module_dtor
BISECT: running pass (144) BDCEPass on __hip_module_dtor
BISECT: running pass (145) SLPVectorizerPass on __hip_module_dtor
BISECT: running pass (146) VectorCombinePass on __hip_module_dtor
BISECT: running pass (147) InferAlignmentPass on __hip_module_dtor
BISECT: running pass (148) InstCombinePass on __hip_module_dtor
BISECT: running pass (149) LoopSimplifyPass on __hip_module_dtor
BISECT: running pass (150) LCSSAPass on __hip_module_dtor
BISECT: running pass (151) AlignmentFromAssumptionsPass on __hip_module_dtor
BISECT: running pass (152) JumpThreadingPass on __hip_module_dtor
BISECT: running pass (153) LoopSimplifyPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (154) LCSSAPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (155) GVNPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (156) MemCpyOptPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (157) DSEPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (158) MoveAutoInitPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (159) MergedLoadStoreMotionPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (160) LoopSimplifyPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (161) LCSSAPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (162) LoopDistributePass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (163) LoopVectorizePass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (164) InferAlignmentPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (165) LoopUnrollPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (166) WarnMissedTransformationsPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (167) SROAPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (168) InstCombinePass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (169) SimplifyCFGPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (170) SCCPPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (171) InstCombinePass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (172) BDCEPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (173) SLPVectorizerPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (174) VectorCombinePass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (175) InferAlignmentPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (176) InstCombinePass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (177) LoopSimplifyPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (178) LCSSAPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (179) AlignmentFromAssumptionsPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (180) JumpThreadingPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (181) LoopSimplifyPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (182) LCSSAPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (183) GVNPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (184) MemCpyOptPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (185) DSEPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (186) MoveAutoInitPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (187) MergedLoadStoreMotionPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (188) LoopSimplifyPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (189) LCSSAPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (190) LoopDistributePass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (191) LoopVectorizePass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (192) InferAlignmentPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (193) LoopUnrollPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (194) WarnMissedTransformationsPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (195) SROAPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (196) InstCombinePass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (197) SimplifyCFGPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (198) SCCPPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (199) InstCombinePass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (200) BDCEPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (201) SLPVectorizerPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (202) VectorCombinePass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (203) InferAlignmentPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (204) InstCombinePass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (205) LoopSimplifyPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (206) LCSSAPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (207) AlignmentFromAssumptionsPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (208) JumpThreadingPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (209) LoopSimplifyPass on main
BISECT: running pass (210) LCSSAPass on main
BISECT: running pass (211) LICMPass on loop %<unnamed loop> in function main
BISECT: running pass (212) GVNPass on main
BISECT: running pass (213) MemCpyOptPass on main
BISECT: running pass (214) DSEPass on main
BISECT: running pass (215) MoveAutoInitPass on main
BISECT: running pass (216) MergedLoadStoreMotionPass on main
BISECT: running pass (217) LoopSimplifyPass on main
BISECT: running pass (218) LCSSAPass on main
BISECT: running pass (219) IndVarSimplifyPass on loop %<unnamed loop> in function main
BISECT: running pass (220) LoopDeletionPass on loop %<unnamed loop> in function main
BISECT: running pass (221) LoopFullUnrollPass on loop %<unnamed loop> in function main
BISECT: running pass (222) LoopDistributePass on main
BISECT: running pass (223) LoopVectorizePass on main
BISECT: running pass (224) InferAlignmentPass on main
BISECT: running pass (225) LoopUnrollPass on main
BISECT: running pass (226) WarnMissedTransformationsPass on main
BISECT: running pass (227) SROAPass on main
BISECT: running pass (228) InstCombinePass on main
BISECT: running pass (229) SimplifyCFGPass on main
BISECT: running pass (230) SCCPPass on main
BISECT: running pass (231) InstCombinePass on main
BISECT: running pass (232) BDCEPass on main
BISECT: running pass (233) SLPVectorizerPass on main
BISECT: running pass (234) VectorCombinePass on main
BISECT: running pass (235) InferAlignmentPass on main
BISECT: running pass (236) InstCombinePass on main
BISECT: running pass (237) LoopSimplifyPass on main
BISECT: running pass (238) LCSSAPass on main
BISECT: running pass (239) LICMPass on loop %<unnamed loop> in function main
BISECT: running pass (240) AlignmentFromAssumptionsPass on main
BISECT: running pass (241) JumpThreadingPass on main
BISECT: running pass (242) LowerTypeTestsPass on [module]
BISECT: running pass (243) LowerTypeTestsPass on [module]
BISECT: running pass (244) LoopSinkPass on __hip_module_ctor
BISECT: running pass (245) DivRemPairsPass on __hip_module_ctor
BISECT: running pass (246) SimplifyCFGPass on __hip_module_ctor
BISECT: running pass (247) LoopSinkPass on __hip_module_dtor
BISECT: running pass (248) DivRemPairsPass on __hip_module_dtor
BISECT: running pass (249) SimplifyCFGPass on __hip_module_dtor
BISECT: running pass (250) LoopSinkPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (251) DivRemPairsPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (252) SimplifyCFGPass on _Z25__device_stub__matrix_mulPdS_S_
BISECT: running pass (253) LoopSinkPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (254) DivRemPairsPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (255) SimplifyCFGPass on _ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_
BISECT: running pass (256) LoopSinkPass on main
BISECT: running pass (257) DivRemPairsPass on main
BISECT: running pass (258) SimplifyCFGPass on main
BISECT: running pass (259) EliminateAvailableExternallyPass on [module]
BISECT: running pass (260) GlobalDCEPass on [module]
BISECT: running pass (261) CGProfilePass on [module]

[scabbard.instr.host.run:DBG] running instrumentation pass on host/CPU module (LTO)
BISECT: running pass (262) Merge contiguous icmps into a memcmp on function (__hip_module_ctor)
BISECT: running pass (263) Expand memcmp() to load/stores on function (__hip_module_ctor)
BISECT: running pass (264) Constant Hoisting on function (__hip_module_ctor)
BISECT: running pass (265) Partially inline calls to library functions on function (__hip_module_ctor)
BISECT: running pass (266) TLS Variable Hoist on function (__hip_module_ctor)
BISECT: running pass (267) X86 Partial Reduction on function (__hip_module_ctor)
BISECT: running pass (268) CodeGen Prepare on function (__hip_module_ctor)
BISECT: running pass (269) X86 DAG->DAG Instruction Selection on function (__hip_module_ctor)
BISECT: running pass (270) Local Dynamic TLS Access Clean-up on function (__hip_module_ctor)
BISECT: running pass (271) X86 Domain Reassignment Pass on function (__hip_module_ctor)
BISECT: running pass (272) Early Tail Duplication on function (__hip_module_ctor)
BISECT: running pass (273) Optimize machine instruction PHIs on function (__hip_module_ctor)
BISECT: running pass (274) Remove dead machine instructions on function (__hip_module_ctor)
BISECT: running pass (275) Early If-Conversion on function (__hip_module_ctor)
BISECT: running pass (276) X86 cmov Conversion on function (__hip_module_ctor)
BISECT: running pass (277) Early Machine Loop Invariant Code Motion on function (__hip_module_ctor)
BISECT: running pass (278) Machine Common Subexpression Elimination on function (__hip_module_ctor)
BISECT: running pass (279) Machine code sinking on function (__hip_module_ctor)
BISECT: running pass (280) Peephole Optimizations on function (__hip_module_ctor)
BISECT: running pass (281) Remove dead machine instructions on function (__hip_module_ctor)
BISECT: running pass (282) Live Range Shrink on function (__hip_module_ctor)
BISECT: running pass (283) X86 LEA Optimize on function (__hip_module_ctor)
BISECT: running pass (284) X86 Optimize Call Frame on function (__hip_module_ctor)
BISECT: running pass (285) X86 Avoid Store Forwarding Blocks on function (__hip_module_ctor)
BISECT: running pass (286) Two-Address instruction pass on function (__hip_module_ctor)
BISECT: running pass (287) Machine Instruction Scheduler on function (__hip_module_ctor)
BISECT: running pass (288) Stack Slot Coloring on function (__hip_module_ctor)
BISECT: running pass (289) Machine Copy Propagation Pass on function (__hip_module_ctor)
BISECT: running pass (290) Machine Loop Invariant Code Motion on function (__hip_module_ctor)
BISECT: running pass (291) Fixup Statepoint Caller Saved on function (__hip_module_ctor)
BISECT: running pass (292) PostRA Machine Sink on function (__hip_module_ctor)
BISECT: running pass (293) Shrink Wrapping analysis on function (__hip_module_ctor)
BISECT: running pass (294) Machine Late Instructions Cleanup Pass on function (__hip_module_ctor)
BISECT: running pass (295) Control Flow Optimizer on function (__hip_module_ctor)
BISECT: running pass (296) Tail Duplication on function (__hip_module_ctor)
BISECT: running pass (297) Machine Copy Propagation Pass on function (__hip_module_ctor)
BISECT: running pass (298) Post RA top-down list latency scheduler on function (__hip_module_ctor)
BISECT: running pass (299) Branch Probability Basic Block Placement on function (__hip_module_ctor)
BISECT: running pass (300) X86 Execution Dependency Fix on function (__hip_module_ctor)
BISECT: running pass (301) BreakFalseDeps on function (__hip_module_ctor)
BISECT: running pass (302) X86 Byte/Word Instruction Fixup on function (__hip_module_ctor)
BISECT: running pass (303) X86 Atom pad short functions on function (__hip_module_ctor)
BISECT: running pass (304) X86 LEA Fixup on function (__hip_module_ctor)
BISECT: running pass (305) Merge contiguous icmps into a memcmp on function (__hip_module_dtor)
BISECT: running pass (306) Expand memcmp() to load/stores on function (__hip_module_dtor)
BISECT: running pass (307) Constant Hoisting on function (__hip_module_dtor)
BISECT: running pass (308) Partially inline calls to library functions on function (__hip_module_dtor)
BISECT: running pass (309) TLS Variable Hoist on function (__hip_module_dtor)
BISECT: running pass (310) X86 Partial Reduction on function (__hip_module_dtor)
BISECT: running pass (311) CodeGen Prepare on function (__hip_module_dtor)
BISECT: running pass (312) X86 DAG->DAG Instruction Selection on function (__hip_module_dtor)
BISECT: running pass (313) Local Dynamic TLS Access Clean-up on function (__hip_module_dtor)
BISECT: running pass (314) X86 Domain Reassignment Pass on function (__hip_module_dtor)
BISECT: running pass (315) Early Tail Duplication on function (__hip_module_dtor)
BISECT: running pass (316) Optimize machine instruction PHIs on function (__hip_module_dtor)
BISECT: running pass (317) Remove dead machine instructions on function (__hip_module_dtor)
BISECT: running pass (318) Early If-Conversion on function (__hip_module_dtor)
BISECT: running pass (319) X86 cmov Conversion on function (__hip_module_dtor)
BISECT: running pass (320) Early Machine Loop Invariant Code Motion on function (__hip_module_dtor)
BISECT: running pass (321) Machine Common Subexpression Elimination on function (__hip_module_dtor)
BISECT: running pass (322) Machine code sinking on function (__hip_module_dtor)
BISECT: running pass (323) Peephole Optimizations on function (__hip_module_dtor)
BISECT: running pass (324) Remove dead machine instructions on function (__hip_module_dtor)
BISECT: running pass (325) Live Range Shrink on function (__hip_module_dtor)
BISECT: running pass (326) X86 LEA Optimize on function (__hip_module_dtor)
BISECT: running pass (327) X86 Optimize Call Frame on function (__hip_module_dtor)
BISECT: running pass (328) X86 Avoid Store Forwarding Blocks on function (__hip_module_dtor)
BISECT: running pass (329) Two-Address instruction pass on function (__hip_module_dtor)
BISECT: running pass (330) Machine Instruction Scheduler on function (__hip_module_dtor)
BISECT: running pass (331) Stack Slot Coloring on function (__hip_module_dtor)
BISECT: running pass (332) Machine Copy Propagation Pass on function (__hip_module_dtor)
BISECT: running pass (333) Machine Loop Invariant Code Motion on function (__hip_module_dtor)
BISECT: running pass (334) Fixup Statepoint Caller Saved on function (__hip_module_dtor)
BISECT: running pass (335) PostRA Machine Sink on function (__hip_module_dtor)
BISECT: running pass (336) Shrink Wrapping analysis on function (__hip_module_dtor)
BISECT: running pass (337) Machine Late Instructions Cleanup Pass on function (__hip_module_dtor)
BISECT: running pass (338) Control Flow Optimizer on function (__hip_module_dtor)
BISECT: running pass (339) Tail Duplication on function (__hip_module_dtor)
BISECT: running pass (340) Machine Copy Propagation Pass on function (__hip_module_dtor)
BISECT: running pass (341) Post RA top-down list latency scheduler on function (__hip_module_dtor)
BISECT: running pass (342) Branch Probability Basic Block Placement on function (__hip_module_dtor)
BISECT: running pass (343) X86 Execution Dependency Fix on function (__hip_module_dtor)
BISECT: running pass (344) BreakFalseDeps on function (__hip_module_dtor)
BISECT: running pass (345) X86 Byte/Word Instruction Fixup on function (__hip_module_dtor)
BISECT: running pass (346) X86 Atom pad short functions on function (__hip_module_dtor)
BISECT: running pass (347) X86 LEA Fixup on function (__hip_module_dtor)
BISECT: running pass (348) Merge contiguous icmps into a memcmp on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (349) Expand memcmp() to load/stores on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (350) Constant Hoisting on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (351) Partially inline calls to library functions on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (352) TLS Variable Hoist on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (353) X86 Partial Reduction on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (354) CodeGen Prepare on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (355) X86 DAG->DAG Instruction Selection on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (356) Local Dynamic TLS Access Clean-up on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (357) X86 Domain Reassignment Pass on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (358) Early Tail Duplication on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (359) Optimize machine instruction PHIs on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (360) Remove dead machine instructions on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (361) Early If-Conversion on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (362) X86 cmov Conversion on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (363) Early Machine Loop Invariant Code Motion on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (364) Machine Common Subexpression Elimination on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (365) Machine code sinking on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (366) Peephole Optimizations on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (367) Remove dead machine instructions on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (368) Live Range Shrink on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (369) X86 LEA Optimize on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (370) X86 Optimize Call Frame on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (371) X86 Avoid Store Forwarding Blocks on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (372) Two-Address instruction pass on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (373) Machine Instruction Scheduler on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (374) Stack Slot Coloring on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (375) Machine Copy Propagation Pass on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (376) Machine Loop Invariant Code Motion on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (377) Fixup Statepoint Caller Saved on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (378) PostRA Machine Sink on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (379) Shrink Wrapping analysis on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (380) Machine Late Instructions Cleanup Pass on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (381) Control Flow Optimizer on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (382) Tail Duplication on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (383) Machine Copy Propagation Pass on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (384) Post RA top-down list latency scheduler on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (385) Branch Probability Basic Block Placement on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (386) X86 Execution Dependency Fix on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (387) BreakFalseDeps on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (388) X86 Byte/Word Instruction Fixup on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (389) X86 Atom pad short functions on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (390) X86 LEA Fixup on function (_Z25__device_stub__matrix_mulPdS_S_)
BISECT: running pass (391) Canonicalize Freeze Instructions in Loops on loop
BISECT: running pass (392) Loop Strength Reduction on loop
BISECT: running pass (393) Merge contiguous icmps into a memcmp on function (main)
BISECT: running pass (394) Expand memcmp() to load/stores on function (main)
BISECT: running pass (395) Constant Hoisting on function (main)
BISECT: running pass (396) Partially inline calls to library functions on function (main)
BISECT: running pass (397) TLS Variable Hoist on function (main)
BISECT: running pass (398) X86 Partial Reduction on function (main)
BISECT: running pass (399) CodeGen Prepare on function (main)
BISECT: running pass (400) X86 DAG->DAG Instruction Selection on function (main)
BISECT: running pass (401) Local Dynamic TLS Access Clean-up on function (main)
BISECT: running pass (402) X86 Domain Reassignment Pass on function (main)
BISECT: running pass (403) Early Tail Duplication on function (main)
BISECT: running pass (404) Optimize machine instruction PHIs on function (main)
BISECT: running pass (405) Merge disjoint stack slots on function (main)
BISECT: running pass (406) Remove dead machine instructions on function (main)
BISECT: running pass (407) Early If-Conversion on function (main)
BISECT: running pass (408) X86 cmov Conversion on function (main)
BISECT: running pass (409) Early Machine Loop Invariant Code Motion on function (main)
BISECT: running pass (410) Machine Common Subexpression Elimination on function (main)
BISECT: running pass (411) Machine code sinking on function (main)
BISECT: running pass (412) Peephole Optimizations on function (main)
BISECT: running pass (413) Remove dead machine instructions on function (main)
BISECT: running pass (414) Live Range Shrink on function (main)
BISECT: running pass (415) X86 LEA Optimize on function (main)
BISECT: running pass (416) X86 Optimize Call Frame on function (main)
BISECT: running pass (417) X86 Avoid Store Forwarding Blocks on function (main)
BISECT: running pass (418) Two-Address instruction pass on function (main)
BISECT: running pass (419) Machine Instruction Scheduler on function (main)
BISECT: running pass (420) Stack Slot Coloring on function (main)
BISECT: running pass (421) Machine Copy Propagation Pass on function (main)
BISECT: running pass (422) Machine Loop Invariant Code Motion on function (main)
BISECT: running pass (423) Fixup Statepoint Caller Saved on function (main)
BISECT: running pass (424) PostRA Machine Sink on function (main)
BISECT: running pass (425) Shrink Wrapping analysis on function (main)
BISECT: running pass (426) Machine Late Instructions Cleanup Pass on function (main)
BISECT: running pass (427) Control Flow Optimizer on function (main)
BISECT: running pass (428) Tail Duplication on function (main)
BISECT: running pass (429) Machine Copy Propagation Pass on function (main)
BISECT: running pass (430) Post RA top-down list latency scheduler on function (main)
BISECT: running pass (431) Branch Probability Basic Block Placement on function (main)
BISECT: running pass (432) X86 Execution Dependency Fix on function (main)
BISECT: running pass (433) BreakFalseDeps on function (main)
BISECT: running pass (434) X86 Byte/Word Instruction Fixup on function (main)
BISECT: running pass (435) X86 Atom pad short functions on function (main)
BISECT: running pass (436) X86 LEA Fixup on function (main)
```


<!-- Notes.md Q1 first draft table -->
<table>
<thead>
  <tr>
    <th><i>Scope</i></th> <th><i>Attribute</i></th> <th><i>Store Loc</i></th> 
    <th><i>Host Vis</i></th> <th><i>Device Vis</i></th>
    <th>When</th>
    <th>C/C++/HIP</th>
    <th>LLVM IR</th> 
  </tr>
</thead>
<tr>
  <td>global </td><td> <code>__managed__</code> </td><td> <i>UNKNOWN</i> </td>
  <td>✔</td> <td>✔</td>
<td>

...Some **md**...
</td><td>

```cpp
__managed__ size_t __g_val__ = 0xFAD; 
```
</td><td>

```llvm
  %5 = call @__g_val__
```
</td></tr>
<tr> 
  <td>global </td><td> <code>__managed__</code> </td><td> <i>UNKNOWN</i>
  <td>✔</td> <td>✔</td>
</td><td>

...Some **md**...
</td><td>

```cpp
__managed__ size_t __g_val__ = 0xFAD; 
```
</td><td>

```llvm
  %5 = call @__g_val__
```
</td>
</tr>
</table>