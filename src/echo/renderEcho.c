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

#include "echo.h"
#include "privateEcho.h"

int
echoThreadStateInit(echoThreadState *tstate,
		    echoRTParm *parm, echoGlobalState *gstate) {
  char me[]="echoThreadStateInit", err[AIR_STRLEN_MED];

  if (!(tstate && parm && gstate)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(ECHO, err); return 1;
  }
  if (nrrdMaybeAlloc(tstate->nperm, nrrdTypeInt, 2,
		     ECHO_JITABLE_NUM, parm->numSamples)) {
    sprintf(err, "%s: couldn't allocate jitter permutation array", me);
    biffMove(ECHO, err, NRRD); return 1;
  }
  nrrdAxesSet(tstate->nperm, nrrdAxesInfoLabel, "info", "sample");
  if (nrrdMaybeAlloc(tstate->njitt, echoPos_nt, 3,
		     2, ECHO_JITABLE_NUM, parm->numSamples)) {
    sprintf(err, "%s: couldn't allocate jitter array", me);
    biffMove(ECHO, err, NRRD); return 1;
  }
  nrrdAxesSet(tstate->njitt, nrrdAxesInfoLabel, "x,y", "info", "sample");
  if (!( tstate->permBuff = (int*)calloc(parm->numSamples, sizeof(int)) )) {
    sprintf(err, "%s: couldn't allocate permutation buffer", me);
    biffAdd(ECHO, err); return 1;
  }
  if (!( tstate->chanBuff =
	 (echoCol_t*)calloc(ECHO_IMG_CHANNELS * parm->numSamples,
			    sizeof(echoCol_t)) )) {
    sprintf(err, "%s: couldn't allocate img channel sample buffer", me);
    biffAdd(ECHO, err); return 1;
  }
  
  return 0;
}

/*
******** echoJitterCompute()
**
**
*/
void
echoJitterCompute(echoRTParm *parm, echoThreadState *tstate) {
  echoPos_t *jitt, w;
  int s, i, j, xi, yi, n, N, *perm;

  N = parm->numSamples;
  n = sqrt(N);
  w = 1.0/n;
  /* each row in perm[] is for one sample, to through all jitables.
     each column is a different permutation of [0..parm->numSamples-1] */
  perm = (int *)tstate->nperm->data;
  for (j=0; j<ECHO_JITABLE_NUM; j++) {
    airShuffle(tstate->permBuff, parm->numSamples, parm->permuteJitter);
    for (s=0; s<N; s++) {
      perm[j + ECHO_JITABLE_NUM*s] = tstate->permBuff[s];
    }
  }
  jitt = (echoPos_t *)tstate->njitt->data;
  for (s=0; s<N; s++) {
    for (j=0; j<ECHO_JITABLE_NUM; j++) {
      i = perm[j + ECHO_JITABLE_NUM*s];
      xi = i % n;
      yi = i / n;
      switch(parm->jitter) {
      case echoJitterNone:
	jitt[0 + 2*j] = 0.0;
	jitt[1 + 2*j] = 0.0;
	break;
      case echoJitterGrid:
	jitt[0 + 2*j] = NRRD_POS(nrrdCenterCell, -0.5, 0.5, n, xi);
	jitt[1 + 2*j] = NRRD_POS(nrrdCenterCell, -0.5, 0.5, n, yi);
	break;
      case echoJitterJitter:
	jitt[0 + 2*j] = NRRD_POS(nrrdCenterCell, -0.5, 0.5, n, xi);
	jitt[0 + 2*j] += AIR_AFFINE(0.0, airRand(), 1.0, -w/2, w/2);
	jitt[1 + 2*j] = NRRD_POS(nrrdCenterCell, -0.5, 0.5, n, yi);
	jitt[1 + 2*j] += AIR_AFFINE(0.0, airRand(), 1.0, -w/2, w/2);
	break;
      case echoJitterRandom:
	jitt[0 + 2*j] = airRand() - 0.5;
	jitt[1 + 2*j] = airRand() - 0.5;
	break;
      }
    }
    jitt += 2*ECHO_JITABLE_NUM;
  }

  return;
}

/*
******** echoCheck
**
** does all the error checking required of echoRender and
** everything that it calls
*/
int
echoCheck(Nrrd *nraw, limnCam *cam, 
	  echoRTParm *parm, echoGlobalState *gstate,
	  echoScene *scene, airArray *lightArr) {
  char me[]="echoCheck", err[AIR_STRLEN_MED];
  int tmp;

  if (!(nraw && cam && parm && gstate && scene && lightArr)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(ECHO, err); return 1;
  }
  if (limnCamUpdate(cam)) {
    sprintf(err, "%s: camera trouble", me);
    biffMove(ECHO, err, LIMN); return 1;
  }
  if (!airEnumValValid(echoJitter, parm->jitter)) {
    sprintf(err, "%s: jitter method (%d) invalid", me, parm->jitter);
    biffAdd(ECHO, err); return 1;
  }
  if (!(parm->numSamples > 0)) {
    sprintf(err, "%s: # samples (%d) invalid", me, parm->numSamples);
    biffAdd(ECHO, err); return 1;
  }
  if (!(parm->imgResU > 0 && parm->imgResV)) {
    sprintf(err, "%s: image dimensions (%dx%d) invalid", me,
	    parm->imgResU, parm->imgResV);
    biffAdd(ECHO, err); return 1;
  }
  if (!AIR_EXISTS(parm->aperture)) {
    sprintf(err, "%s: aperture doesn't exist", me);
    biffAdd(ECHO, err); return 1;
  }
  
  switch (parm->jitter) {
  case echoJitterNone:
  case echoJitterRandom:
    break;
  case echoJitterGrid:
  case echoJitterJitter:
    tmp = sqrt(parm->numSamples);
    if (tmp*tmp != parm->numSamples) {
      sprintf(err, "%s: need a square # samples for %s jitter method (not %d)",
	      me, airEnumStr(echoJitter, parm->jitter), parm->numSamples);
      biffAdd(ECHO, err); return 1;
    }
    break;
  }
  
  if (ECHO_IMG_CHANNELS != 5) {
    sprintf(err, "%s: ECHO_IMG_CHANNELS != 5", me);
    biffAdd(ECHO, err); return 1;
  }
  
  /* all is well */
  return 0;
}

void
echoChannelAverage(echoCol_t *img,
		   echoRTParm *parm, echoThreadState *tstate) {
  int s;
  echoCol_t R, G, B, A, T;
  
  R = G = B = A = T = 0;
  for (s=0; s<parm->numSamples; s++) {
    R += tstate->chanBuff[0 + ECHO_IMG_CHANNELS*s];
    G += tstate->chanBuff[1 + ECHO_IMG_CHANNELS*s];
    B += tstate->chanBuff[2 + ECHO_IMG_CHANNELS*s];
    A += tstate->chanBuff[3 + ECHO_IMG_CHANNELS*s];
    T += tstate->chanBuff[4 + ECHO_IMG_CHANNELS*s];
  }
  img[0] = R / parm->numSamples;
  img[1] = G / parm->numSamples;
  img[2] = B / parm->numSamples;
  img[3] = A / parm->numSamples;
  img[4] = T;
  
  return;
}

/*
** echoRayColor
**
** This is called by echoRender and by the various color routines,
** following an intersection with non-phong non-light material.
** As such, it is never called on shadow rays.  
*/
void
echoRayColor(echoCol_t *chan, int samp, echoRay *ray,
	     echoRTParm *parm, echoThreadState *tstate,
	     echoObject *scene, airArray *lightArr) {
  float tmp;
  echoIntx intx;
  
  if (ray->depth > parm->maxRecDepth) {
    /* we've exceeded the recursion depth, so no more rays for you */
    ELL_4V_SET(chan, parm->mrR, parm->mrG, parm->mrB, 1.0);
    return;
  }

  intx.boxhits = 0;
  if (!echoRayIntx(&intx, ray, parm, scene)) {
    if (echoVerbose) {
      printf("echoRayColor: (nothing was hit)\n");
    }
    /* ray hits nothing in scene */
    if (parm->renderBoxes) {
      tmp = 0.1*intx.boxhits;
      ELL_4V_SET(chan, tmp, tmp, tmp, 1.0);
    }
    else {
      ELL_4V_SET(chan, parm->bgR, parm->bgG, parm->bgB, 1.0);
    }
    return;
  }

  /* else we actually hit something.  Chances are, we'll need to
     know the view vector and intersection location ("pos"), either
     for texturing or for starting new rays, so we'll calculate 
     those here.  Also, record the depth for sake of child rays 
     generated by _echoIntxColor */
  ELL_3V_SCALE(intx.view, -1, ray->dir);
  ELL_3V_SCALEADD(intx.pos, 1, ray->from, intx.t, ray->dir);
  if (echoVerbose) {
    printf("echoRayColor: hit a %d (%p) at (%g,%g,%g)\n",
	   intx.obj->type, intx.obj,
	   intx.pos[0], intx.pos[1], intx.pos[2]);
  }
  intx.depth = ray->depth;
  _echoIntxColor[intx.obj->matter](chan, &intx, samp, parm,
				   tstate, scene, lightArr);

  return;
}

/*
******** echoRTRender
**
** top-level call to accomplish all (ray-tracing) rendering.  As much
** error checking as possible should be done here and not in the
** lower-level functions.
*/
int
echoRTRender(Nrrd *nraw, limnCam *cam,
	     echoRTParm *parm, echoGlobalState *gstate,
	     echoObject *scene, airArray *lightArr) {
  char me[]="echoRTRender", err[AIR_STRLEN_MED], done[20];
  int imgUi, imgVi,         /* integral pixel indices */
    samp;                   /* which sample are we doing */
  echoPos_t tmp0, tmp1,
    eye[3],                 /* eye center before jittering */
    at[3],                  /* ray destination (pixel center post-jittering) */
    U[4], V[4], N[4],       /* view space basis (only first 3 elements used) */
    pixUsz, pixVsz,         /* U and V dimensions of a pixel */
    imgU, imgV,             /* floating point pixel center locations */
    imgOrig[3],             /* image origin */
    *jitt;                  /* current scanline of master jitter array */
  echoThreadState *tstate;  /* only one thread for now */
  echoCol_t *img, *chan;    /* current scanline of channel buffer array */
  double time0;             /* to record per ray sample time */
  echoRay ray;              /* (not a pointer) */

  if (echoCheck(nraw, cam, parm, gstate, scene, lightArr)) {
    sprintf(err, "%s: problem with input", me);
    biffAdd(ECHO, err); return 1;
  }
  if (nrrdMaybeAlloc(nraw, echoCol_nt, 3,
		     ECHO_IMG_CHANNELS, parm->imgResU, parm->imgResV)) {
    sprintf(err, "%s: couldn't allocate output image", me);
    biffMove(ECHO, err, NRRD); return 1;
  }
  nrrdAxesSet(nraw, nrrdAxesInfoLabel, "r,g,b,a,t", "x", "y");
  tstate = echoThreadStateNew();
  if (echoThreadStateInit(tstate, parm, gstate)) {
    sprintf(err, "%s:", me);
    biffAdd(ECHO, err); return 1;
  }

  gstate->time = airTime();
  if (parm->seedRand) {
    airSrand();
  }
  echoJitterCompute(parm, tstate);
  if (echoVerbose > 2)
    nrrdSave("jitt.nrrd", tstate->njitt, NULL);
  
  /* set eye, U, V, N, imgOrig */
  ELL_3V_COPY(eye, cam->from);
  ELL_4MV_ROW0_GET(U, cam->W2V);
  ELL_4MV_ROW1_GET(V, cam->W2V);
  ELL_4MV_ROW2_GET(N, cam->W2V);
  ELL_3V_SCALEADD(imgOrig, 1.0, eye, cam->vspDist, N);
  
  /* determine pixel dimensions */
  pixUsz = (cam->uRange[1] - cam->uRange[0])/(parm->imgResU);
  pixVsz = (cam->vRange[1] - cam->vRange[0])/(parm->imgResV);

  ray.depth = 0;
  ray.shadow = AIR_FALSE;
  img = (echoCol_t *)nraw->data;
  printf("      ");
  for (imgVi=0; imgVi<parm->imgResV; imgVi++) {
    imgV = NRRD_POS(nrrdCenterCell, cam->vRange[0], cam->vRange[1],
		    parm->imgResV, imgVi);
    printf("%s", airDoneStr(0, imgVi, parm->imgResV-1, done)); fflush(stdout);
    for (imgUi=0; imgUi<parm->imgResU; imgUi++) {
      imgU = NRRD_POS(nrrdCenterCell, cam->uRange[0], cam->uRange[1],
		      parm->imgResU, imgUi);

      echoVerbose = ( (205 == imgUi && 103 == imgVi) ||
		      (0 && 150 == imgUi && 232 == imgVi) ||
		      (0 && 149 == imgUi && 233 == imgVi) ||
		      (0 && 150 == imgUi && 233 == imgVi) );
      
      if (echoVerbose) {
	printf("\n");
	printf("-----------------------------------------------------\n");
	printf("------------------- (%3d, %3d) ----------------------\n",
	       imgUi, imgVi);
	printf("-----------------------------------------------------\n\n");
      }

      /* initialize things on first "scanline" */
      jitt = (echoPos_t *)tstate->njitt->data;
      chan = tstate->chanBuff;

      /* go through samples */
      for (samp=0; samp<parm->numSamples; samp++) {
	/* set ray.from[] */
	ELL_3V_COPY(ray.from, eye);
	if (parm->aperture) {
	  tmp0 = parm->aperture*jitt[0 + 2*echoJitableLens];
	  tmp1 = parm->aperture*jitt[1 + 2*echoJitableLens];
	  ELL_3V_SCALEADD3(ray.from, 1, ray.from, tmp0, U, tmp1, V);
	}
	
	/* set at[] */
	tmp0 = imgU + pixUsz*jitt[0 + 2*echoJitablePixel];
	tmp1 = imgV + pixVsz*jitt[1 + 2*echoJitablePixel];
	ELL_3V_SCALEADD3(at, 1, imgOrig, tmp0, U, tmp1, V);

	/* do it! */
	ELL_3V_SUB(ray.dir, at, ray.from);
	ray.neer = 0.0;
	ray.faar = ECHO_POS_MAX;
	/*
	printf("!%s:(%d,%d): from=(%g,%g,%g); at=(%g,%g,%g); dir=(%g,%g,%g)\n",
	       me, imgUi, imgVi, from[0], from[1], from[2],
	       at[0], at[1], at[2], dir[0], dir[1], dir[2]);
	*/
	time0 = airTime();
#if 1
	echoRayColor(chan, samp, &ray,
		     parm, tstate,
		     scene, lightArr);
#else
	memset(chan, 0, ECHO_IMG_CHANNELS*sizeof(echoCol_t));
#endif
	chan[4] = airTime() - time0;
	
	/* move to next "scanlines" */
	jitt += 2*ECHO_JITABLE_NUM;
	chan += ECHO_IMG_CHANNELS;
      }
      echoChannelAverage(img, parm, tstate);
      img += ECHO_IMG_CHANNELS;
      if (!parm->reuseJitter) 
	echoJitterCompute(parm, tstate);
    }
  }
  gstate->time = airTime() - gstate->time;
  
  tstate = echoThreadStateNix(tstate);

  return 0;
}
