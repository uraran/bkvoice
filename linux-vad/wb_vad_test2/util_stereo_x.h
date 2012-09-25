#ifndef _UTIL_STEREO_X_H
#define _UTIL_STEREO_X_H

typedef struct
{
     int vdim;
     int nstages;
     int intens;
     int *cbsizes;
     float **cbs;
} MSVQ;

typedef struct
{
     float a;              /* Prediction coefficient */
     float a_fe;           /* Prediction coefficient to be applied in decoder in case of fe */
     float *mean;          /* Mean vector */
     MSVQ msvq;
} PMSVQ;


#endif
