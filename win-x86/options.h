#ifndef options_h
#define options_h


/* Define LAGW in order to use Gaussian Lag Windowing */
#define LAGW
/* Define COSINE_INTERP in order to use sample based cosine interpolation */
#define COSINE_INTERP
/* Define USE_CHOLESKY in order to use Cholesky instead of Levinson algorithm */
#define USE_CHOLESKY

/* AMR-WB+ input/output sampling rate filters */
/* Choice of filter should be selected according to the hardware specification */
/* Both can be selected if required */
/* FILTER_44khz allow 11025, 22050 and 44100 Hz sampling rates */
/* FILTER_48khz allow 8, 16, 24, 32 and 48 kHz sampling rates */
#define FILTER_44kHz
#define FILTER_48kHz

#endif


