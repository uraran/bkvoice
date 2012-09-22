gcc -c -I./inc -I../vo-amrwbenc-0.1.2/common/include/ \
src/autocorr.c  src/cor_h_x.c  src/homing.c   src/isp_az.c    src/math_op.c    src/pred_lt4.c  src/random.c    src/util.c \
src/az_isp.c    src/decim54.c  src/hp400.c    src/isp_isf.c   src/mem_align.c  src/preemph.c   src/residu.c    src/voAMRWBEnc.c \
src/bits.c      src/deemph.c   src/hp50.c     src/lag_wind.c  src/oper_32b.c   src/q_gain2.c   src/scale.c     src/voicefac.c \
src/c2t64fx.c   src/dtx.c      src/hp6k.c     src/levinson.c  src/pitch_f4.c   src/qisf_ns.c   src/stream.c    src/wb_vad.c \
src/c4t64fx.c   src/gpclip.c   src/hp_wsp.c   src/log2.c      src/pit_shrp.c   src/qpisf_2s.c  src/syn_filt.c  src/weight_a.c \
src/convolve.c  src/g_pitch.c  src/int_lpc.c  src/lp_dec2.c   src/p_med_ol.c   src/q_pulse.c   src/updt_tar.c








arm-linux-gcc -c -I./inc -I../vo-amrwbenc-0.1.2/common/include/ -DASM_OPT \
src/autocorr.c  src/cor_h_x.c  src/homing.c   src/isp_az.c    src/math_op.c    src/pred_lt4.c  src/random.c    src/util.c \
src/az_isp.c    src/decim54.c  src/hp400.c    src/isp_isf.c   src/mem_align.c  src/preemph.c   src/residu.c    src/voAMRWBEnc.c \
src/bits.c      src/deemph.c   src/hp50.c     src/lag_wind.c  src/oper_32b.c   src/q_gain2.c   src/scale.c     src/voicefac.c \
src/c2t64fx.c   src/dtx.c      src/hp6k.c     src/levinson.c  src/pitch_f4.c   src/qisf_ns.c   src/stream.c    src/wb_vad.c \
src/c4t64fx.c   src/gpclip.c   src/hp_wsp.c   src/log2.c      src/pit_shrp.c   src/qpisf_2s.c  src/syn_filt.c  src/weight_a.c \
src/convolve.c  src/g_pitch.c  src/int_lpc.c  src/lp_dec2.c   src/p_med_ol.c   src/q_pulse.c   src/updt_tar.c




arm-linux-gcc -o test -I./inc -I./common/include/ -DASM_OPT \
src/autocorr.c  src/cor_h_x.c  src/homing.c   src/isp_az.c    src/math_op.c    src/pred_lt4.c  src/random.c    src/util.c \
src/az_isp.c    src/decim54.c  src/hp400.c    src/isp_isf.c   src/mem_align.c  src/preemph.c   src/residu.c    src/voAMRWBEnc.c \
src/bits.c      src/deemph.c   src/hp50.c     src/lag_wind.c  src/oper_32b.c   src/q_gain2.c   src/scale.c     src/voicefac.c \
src/c2t64fx.c   src/dtx.c      src/hp6k.c     src/levinson.c  src/pitch_f4.c   src/qisf_ns.c   src/stream.c    src/wb_vad.c \
src/c4t64fx.c   src/gpclip.c   src/hp_wsp.c   src/log2.c      src/pit_shrp.c   src/qpisf_2s.c  src/syn_filt.c  src/weight_a.c \
src/convolve.c  src/g_pitch.c  src/int_lpc.c  src/lp_dec2.c   src/p_med_ol.c   src/q_pulse.c   src/updt_tar.c \
src/asm/ARMV5E/convolve_opt.s   src/asm/ARMV5E/Dot_p_opt.s       src/asm/ARMV5E/pred_lt4_1_opt.s  src/asm/ARMV5E/Syn_filt_32_opt.s \
src/asm/ARMV5E/cor_h_vec_opt.s  src/asm/ARMV5E/Filt_6k_7k_opt.s  src/asm/ARMV5E/residu_asm_opt.s  src/asm/ARMV5E/syn_filt_opt.s \
src/asm/ARMV5E/Deemph_32_opt.s  src/asm/ARMV5E/Norm_Corr_opt.s   src/asm/ARMV5E/scale_sig_opt.s \
common/cmnMemory.c \
test.c


