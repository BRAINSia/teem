/*
  teem: Gordon Kindlmann's research software
  Copyright (C) 2002, 2001, 2000, 1999, 1998 University of Utah

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include "nrrd.h"

int
_nrrdCM_median(float *hist, float half) {
  float sum = 0, *hpt;
  
  hpt = hist;
  
  while(sum < half)
    sum += *hpt++;
  
  return(hpt - 1 - hist);
}

#define INDEX(nin, lup, idxIn, bins, val, idxOut) ( \
  val = (lup)((nin)->data, (idxIn)), \
  AIR_INDEX((nin)->min, (val), (nin)->max, bins, (idxOut)), \
  (idxOut) \
)

void
_nrrdCM_printhist(float *hist, int bins, char *desc) {
  int i;

  printf("%s:\n", desc);
  for (i=0; i<=bins-1; i++) {
    if (hist[i]) {
      printf("   %d: %g\n", i, hist[i]);
    }
  }
}

float *
_nrrdCM_wtAlloc(int radius, float wght) {
  float *wt, sum;
  int diam, r;
  
  diam = 2*radius + 1;
  wt = calloc(diam, sizeof(float));
  wt[radius] = 1.0;
  for (r=1; r<=radius; r++) {
    wt[radius+r] = pow(1.0/wght, r);
    wt[radius-r] = pow(1.0/wght, r);
  }
  sum = 0.0;
  for (r=0; r<=diam-1; r++) {
    sum += wt[r];
  }
  for (r=0; r<=diam-1; r++) {
    wt[r] /= sum;
  }
  /*
  for (r=0; r<=diam-1; r++) {
    fprintf(stderr, "%s: wt[%d] = %g\n", "_nrrdCM_wtAlloc", r, wt[r]);
  }
  */
  return wt;
}

void
_nrrdCheapMedian1D(Nrrd *nout, Nrrd *nin, int radius, float wght,
		   int bins, float *hist) {
  /* char me[]="_nrrdCheapMedian1D"; */
  nrrdBigInt num;
  int X, I, idx, diam;
  float half, *wt;
  double val, (*lup)(void *, nrrdBigInt);

  diam = 2*radius + 1;
  lup = nrrdDLookup[nin->type];
  num = nrrdElementNumber(nin);
  if (1 == wght) {
    /* uniform weighting-> can do sliding histogram optimization */
    /* initialize histogram */
    half = diam/2 + 1;
    memset(hist, 0, bins*sizeof(float));
    for (X=0; X<=diam-1; X++) {
      hist[INDEX(nin, lup, X, bins, val, idx)]++;
    }
    /* _nrrdCM_printhist(hist, bins, "after init"); */
    /* find median at each point using existing histogram */
    for (X=radius; X<=num-radius-1; X++) {
      /* _nrrdCM_printhist(hist, bins, "----------"); */
      idx = _nrrdCM_median(hist, half);
      val = NRRD_AXIS_POS(nrrdCenterCell, nin->min, nin->max, bins, idx);
      /* printf(" median idx = %d -> val = %g\n", idx, val); */
      nrrdDInsert[nout->type](nout->data, X, val);
      /* probably update histogram for next iteration */
      if (X < num-radius-1) {
	hist[INDEX(nin, lup, X+radius+1, bins, val, idx)]++;
	hist[INDEX(nin, lup, X-radius, bins, val, idx)]--;
      }
    }
  }
  else {
    /* non-uniform weighting --> slow and stupid */
    wt = _nrrdCM_wtAlloc(radius, wght);
    half = 0.5;
    for (X=radius; X<=num-radius-1; X++) {
      memset(hist, 0, bins*sizeof(float));
      for (I=-radius; I<=radius; I++) {
	hist[INDEX(nin, lup, I+X, bins, val, idx)] += wt[I+radius];
      }
      idx = _nrrdCM_median(hist, half);
      val = NRRD_AXIS_POS(nrrdCenterCell, nin->min, nin->max, bins, idx);
      nrrdDInsert[nout->type](nout->data, X, val);
    }
    free(wt);
  }
}

void
_nrrdCheapMedian2D(Nrrd *nout, Nrrd *nin, int radius, float wght,
		   int bins, float *hist) {
  /* char me[]="_nrrdCheapMedian2D"; */
  int X, Y, I, J;
  int sx, sy, idx, diam;
  float half, *wt;
  double val, (*lup)(void *, nrrdBigInt);

  diam = 2*radius + 1;
  sx = nin->axis[0].size;
  sy = nin->axis[1].size;
  lup = nrrdDLookup[nin->type];
  if (1 == wght) {
    /* uniform weighting-> can do sliding histogram optimization */
    half = diam*diam/2 + 1;
    for (Y=radius; Y<=sy-radius-1; Y++) {
      /* initialize histogram */
      memset(hist, 0, bins*sizeof(float));
      X = radius;
      for (J=-radius; J<=radius; J++) {
	for (I=-radius; I<=radius; I++) {
	  hist[INDEX(nin, lup, X+I + sx*(J+Y), bins, val, idx)]++;
	}
      }
      /* _nrrdCM_printhist(hist, bins, "after init"); */
      /* find median at each point using existing histogram */
      for (X=radius; X<=sx-radius-1; X++) {
	idx = _nrrdCM_median(hist, half);
	val = NRRD_AXIS_POS(nrrdCenterCell, nin->min, nin->max, bins, idx);
	nrrdDInsert[nout->type](nout->data, X + sx*Y, val);
	/* probably update histogram for next iteration */
	if (X < sx-radius-1) {
	  for (J=-radius; J<=radius; J++) {
	    hist[INDEX(nin, lup, X+radius+1 + sx*(J+Y), bins, val, idx)]++;
	    hist[INDEX(nin, lup, X-radius + sx*(J+Y), bins, val, idx)]--;
	  }
	}
      }
    }
  }
  else {
    /* non-uniform weighting --> slow and stupid */
    wt = _nrrdCM_wtAlloc(radius, wght);
    half = 0.5;
    for (Y=radius; Y<=sy-radius-1; Y++) {
      for (X=radius; X<=sx-radius-1; X++) {
	memset(hist, 0, bins*sizeof(float));
	for (J=-radius; J<=radius; J++) {
	  for (I=-radius; I<=radius; I++) {
	    hist[INDEX(nin, lup, I+X + sx*(J+Y),
		       bins, val, idx)] += wt[I+radius]*wt[J+radius];
	  }
	}
	idx = _nrrdCM_median(hist, half);
	val = NRRD_AXIS_POS(nrrdCenterCell, nin->min, nin->max, bins, idx);
	nrrdDInsert[nout->type](nout->data, X + sx*Y, val);
      }
    }
    free(wt);
  }
}

void
_nrrdCheapMedian3D(Nrrd *nout, Nrrd *nin, int radius, float wght,
		   int bins, float *hist) {
  char me[]="_nrrdCheapMedian3D";
  int X, Y, Z, I, J, K;
  int sx, sy, sz, idx, diam;
  float half, *wt;
  double val, (*lup)(void *, nrrdBigInt);

  diam = 2*radius + 1;
  sx = nin->axis[0].size;
  sy = nin->axis[1].size;
  sz = nin->axis[2].size;
  lup = nrrdDLookup[nin->type];
  if (1 == wght) {
    /* uniform weighting-> can do sliding histogram optimization */
    half = diam*diam*diam/2 + 1;
    for (Z=radius; Z<=sz-radius-1; Z++) {
      for (Y=radius; Y<=sy-radius-1; Y++) {
	/* initialize histogram */
	memset(hist, 0, bins*sizeof(float));
	X = radius;
	for (K=-radius; K<=radius; K++) {
	  for (J=-radius; J<=radius; J++) {
	    for (I=-radius; I<=radius; I++) {
	      hist[INDEX(nin, lup, I+X + sx*(J+Y + sy*(K+Z)),
			 bins, val, idx)]++;
	    }
	  }
	}
	/* find median at each point using existing histogram */
	for (X=radius; X<=sx-radius-1; X++) {
	  idx = _nrrdCM_median(hist, half);
	  val = NRRD_AXIS_POS(nrrdCenterCell, nin->min, nin->max, bins, idx);
	  nrrdDInsert[nout->type](nout->data, X + sx*(Y + sy*Z), val);
	  /* probably update histogram for next iteration */
	  if (X < sx-radius-1) {
	    for (K=-radius; K<=radius; K++) {
	      for (J=-radius; J<=radius; J++) {
		hist[INDEX(nin, lup, X+radius+1 + sx*(J+Y + sy*(K+Z)),
			   bins, val, idx)]++;
		hist[INDEX(nin, lup, X-radius + sx*(J+Y + sy*(K+Z)),
			   bins, val, idx)]--;
	      }
	    }
	  }
	}
      }
    }
  }
  else {
    /* non-uniform weighting --> slow and stupid */
    wt = _nrrdCM_wtAlloc(radius, wght);
    half = 0.5;
    for (Z=radius; Z<=sz-radius-1; Z++) {
      fprintf(stderr, "%s: Z = %d/%d\n", me, Z-radius, sz-2*radius-1);
      for (Y=radius; Y<=sy-radius-1; Y++) {
	for (X=radius; X<=sx-radius-1; X++) {
	  memset(hist, 0, bins*sizeof(float));
	  for (K=-radius; K<=radius; K++) {
	    for (J=-radius; J<=radius; J++) {
	      for (I=-radius; I<=radius; I++) {
		hist[INDEX(nin, lup, I+X + sx*(J+Y + sy*(K+Z)),
			   bins, val, idx)]
		  += wt[I+radius]*wt[J+radius]*wt[K+radius];
	      }
	    }
	  }
	  idx = _nrrdCM_median(hist, half);
	  val = NRRD_AXIS_POS(nrrdCenterCell, nin->min, nin->max, bins, idx);
	  nrrdDInsert[nout->type](nout->data, X + sx*(Y + sy*Z), val);
	}
      }
    }
    free(wt);
  }
}


int
nrrdCheapMedian(Nrrd *nout, Nrrd *nin, int radius, float wght, int bins) {
  char me[]="nrrdCheapMedian", err[AIR_STRLEN_MED];
  float *hist;

  if (!(nin && nout)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(NRRD, err); return 1;
  }
  if (!(radius >= 1)) {
    sprintf(err, "%s: need radius >= 1 (got %d)", me, radius);
    biffAdd(NRRD, err); return 1;
  }
  if (!(bins >= 1)) {
    sprintf(err, "%s: need bins >= 1 (got %d)", me, bins);
    biffAdd(NRRD, err); return 1;
  }
  if (!(AIR_INSIDE(1, nin->dim, 3))) {
    sprintf(err, "%s: sorry, can only handle dim 1, 2, 3 (not %d)", 
	    me, nin->dim);
    biffAdd(NRRD, err); return 1;    
  }
  if (nrrdCopy(nout, nin)) {
    sprintf(err, "%s: failed to create copy of input", me);
    biffAdd(NRRD, err); return 1;
  }
  if (nrrdCleverMinMax(nin)) {
    sprintf(err, "%s: couldn't learn value range", me);
    biffAdd(NRRD, err); return 1;
  }
  if (!(hist = calloc(bins, sizeof(float)))) {
    sprintf(err, "%s: couldn't allocate histogram (%d bins)", me, bins);
    biffAdd(NRRD, err); return 1;
  }
  if (!AIR_EXISTS(wght))
    wght = 1.0;
  switch (nin->dim) {
  case 1:
    _nrrdCheapMedian1D(nout, nin, radius, wght, bins, hist);
    break;
  case 2:
    _nrrdCheapMedian2D(nout, nin, radius, wght, bins, hist);
    break;
  case 3:
    _nrrdCheapMedian3D(nout, nin, radius, wght, bins, hist);
    break;
  default:
    sprintf(err, "%s: sorry, %d-dimensional median unimplemented",
	    me, nin->dim);
    biffAdd(NRRD, err); return 1;
  }

  nrrdAxesCopy(nout, nin, NULL, NRRD_AXESINFO_NONE);
  nout->content = airFree(nout->content);
  if (nin->content) {
    nout->content = calloc(strlen("cheapmedian(,,)")
			   + strlen(nin->content)
			   + 11
			   + 11
			   + 11
			   + 1, sizeof(char));
    if (nout->content) {
      sprintf(nout->content, "cheapmedian(%s,%d,%g,%d)", 
	      nin->content, radius, wght, bins);
    }
    else {
      sprintf(err, "%s: couldn't allocate output content", me);
      biffAdd(NRRD, err); return 1;
    }
  }

  hist = airFree(hist);
  return 0;
}

