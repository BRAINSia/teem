/*
  Teem: Tools to process and visualize scientific data and images              
  Copyright (C) 2008, 2007, 2006, 2005  Gordon Kindlmann
  Copyright (C) 2004, 2003, 2002, 2001, 2000, 1999, 1998  University of Utah

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public License
  (LGPL) as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  The terms of redistributing and/or modifying this software also
  include exceptions to the LGPL that facilitate static linking.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this library; if not, write to Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "pull.h"
#include "privatePull.h"

gageKind *
pullGageKindParse(const char *_str) {
  char me[]="pullGageKindParse", err[BIFF_STRLEN], *str;
  gageKind *ret;
  airArray *mop;
  
  if (!_str) {
    sprintf(err, "%s: got NULL pointer", me);
    return NULL;
  }
  mop = airMopNew();
  str = airStrdup(_str);
  airMopAdd(mop, str, airFree, airMopAlways);
  airToLower(str);
  if (!strcmp(gageKindScl->name, str)) {
    ret = gageKindScl;
  } else if (!strcmp(gageKindVec->name, str)) {
    ret = gageKindVec;
  } else if (!strcmp(tenGageKind->name, str)) {
    ret = tenGageKind;
  } else /* not allowing DWIs for now */ {
    sprintf(err, "%s: not \"%s\", \"%s\", or \"%s\"", me,
            gageKindScl->name, gageKindVec->name, tenGageKind->name);
    airMopError(mop); return NULL;
  }

  airMopOkay(mop);
  return ret;
}

pullVolume *
pullVolumeNew() {
  pullVolume *vol;

  vol = AIR_CAST(pullVolume *, calloc(1, sizeof(pullVolume)));
  if (vol) {
    vol->verbose = 0;
    vol->name = NULL;
    vol->kind = NULL;
    vol->ninSingle = NULL;
    vol->ninScale = NULL;
    vol->scaleNum = 0;
    vol->scalePos = NULL;
    vol->ksp00 = nrrdKernelSpecNew();
    vol->ksp11 = nrrdKernelSpecNew();
    vol->ksp22 = nrrdKernelSpecNew();
    vol->kspSS = nrrdKernelSpecNew();
    vol->gctx = gageContextNew();
    vol->gpvl = NULL;
    vol->gpvlSS = NULL;
    vol->seedOnly = AIR_TRUE;
  }
  return vol;
}

pullVolume *
pullVolumeNix(pullVolume *vol) {

  if (vol) {
    vol->name = airFree(vol->name);
    airFree(vol->scalePos);
    vol->ksp00 = nrrdKernelSpecNix(vol->ksp00);
    vol->ksp11 = nrrdKernelSpecNix(vol->ksp11);
    vol->ksp22 = nrrdKernelSpecNix(vol->ksp22);
    vol->kspSS = nrrdKernelSpecNix(vol->kspSS);
    vol->gctx = gageContextNix(vol->gctx);
    airFree(vol->gpvlSS);
    airFree(vol);
  }
  return NULL;
}

/*
** used to set all the fields of pullVolume at once, including the
** gageContext inside the pullVolume
**
** used both for top-level volumes in the pullContext (pctx->vol[i])
** in which case pctx is non-NULL,
** and for the per-task volumes (task->vol[i]),
** in which case pctx is NULL
*/
int
_pullVolumeSet(pullContext *pctx, pullVolume *vol,
               int verbose, const char *name,
               const Nrrd *ninSingle,
               const Nrrd *const *ninScale, double *scalePos,
               unsigned int ninNum,
               const gageKind *kind, 
               const NrrdKernelSpec *ksp00,
               const NrrdKernelSpec *ksp11,
               const NrrdKernelSpec *ksp22,
               const NrrdKernelSpec *kspSS) {
  char me[]="_pullVolumeSet", err[BIFF_STRLEN];
  int E;
  unsigned int vi;

  if (!( vol && ninSingle && kind 
         && airStrlen(name) && ksp00 && ksp11 && ksp22 )) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(PULL, err); return 1;
  }
  if (pctx) {
    for (vi=0; vi<pctx->volNum; vi++) {
      if (pctx->vol[vi] == vol) {
        sprintf(err, "%s: already got vol %p as vol[%u]", me, vol, vi);
        biffAdd(PULL, err); return 1;
      }
    }
  }
  if (ninScale && !scalePos) {
    sprintf(err, "%s: need scalePos array if using scale-space", me);
    biffAdd(PULL, err); return 1;
  }
  if (ninScale && !(ninNum >= 2)) {
    sprintf(err, "%s: need at least 2 volumes (not %u)", me, ninNum);
    biffAdd(PULL, err); return 1;
  }
  if (!(vol->gctx)) {
    sprintf(err, "%s: got NULL vol->gageContext", me);
    biffAdd(PULL, err); return 1;
  }
  vol->verbose = verbose;
  gageParmSet(vol->gctx, gageParmVerbose,
              vol->verbose > 0 ? vol->verbose - 1 : 0);
  gageParmSet(vol->gctx, gageParmRenormalize, AIR_FALSE);
  /* because we're likely only using accurate kernels */
  gageParmSet(vol->gctx, gageParmStackNormalizeRecon, AIR_FALSE);
  gageParmSet(vol->gctx, gageParmCheckIntegrals, AIR_TRUE);
  E = 0;
  if (!E) E |= gageKernelSet(vol->gctx, gageKernel00,
                             ksp00->kernel, ksp00->parm);
  if (!E) E |= gageKernelSet(vol->gctx, gageKernel11,
                             ksp11->kernel, ksp11->parm); 
  if (!E) E |= gageKernelSet(vol->gctx, gageKernel22,
                             ksp22->kernel, ksp22->parm);
  if (ninScale) {
    if (!kspSS) {
      sprintf(err, "%s: got NULL kspSS", me);
      biffAdd(PULL, err); return 1;
    }
    gageParmSet(vol->gctx, gageParmStackUse, AIR_TRUE);
    if (!E) E |= !(vol->gpvl = gagePerVolumeNew(vol->gctx, ninSingle, kind));
    if (!E) E |= gageStackPerVolumeNew(vol->gctx, &(vol->gpvlSS),
                                       ninScale, ninNum, kind);
    if (!E) E |= gageStackPerVolumeAttach(vol->gctx, vol->gpvl, vol->gpvlSS,
                                          scalePos, ninNum);
    if (!E) E |= gageKernelSet(vol->gctx, gageKernelStack,
                               kspSS->kernel, kspSS->parm);
  } else {
    if (!E) E |= !(vol->gpvl = gagePerVolumeNew(vol->gctx, ninSingle, kind));
    if (!E) E |= gagePerVolumeAttach(vol->gctx, vol->gpvl);
  }
  if (E) {
    sprintf(err, "%s: trouble", me);
    biffMove(PULL, err, GAGE); return 1;
  }

  vol->name = airStrdup(name);
  if (!vol->name) {
    sprintf(err, "%s: couldn't strdup name (len %u)", me,
            AIR_CAST(unsigned int, airStrlen(name)));
    biffAdd(PULL, err); return 1;
  }
  printf("!%s: vol=%p, name = %p = |%s|\n", me, vol, 
         vol->name, vol->name);
  vol->kind = kind;
  nrrdKernelSpecSet(vol->ksp00, ksp00->kernel, ksp00->parm);
  nrrdKernelSpecSet(vol->ksp11, ksp11->kernel, ksp11->parm);
  nrrdKernelSpecSet(vol->ksp22, ksp22->kernel, ksp22->parm);
  if (ninScale) {
    vol->ninSingle = ninSingle;
    vol->ninScale = ninScale;
    vol->scaleNum = ninNum;
    vol->scalePos = AIR_CAST(double *, calloc(ninNum, sizeof(double)));
    if (!vol->scalePos) {
      sprintf(err, "%s: couldn't calloc scalePos", me);
      biffAdd(PULL, err); return 1;
    }
    for (vi=0; vi<ninNum; vi++) {
      vol->scalePos[vi] = scalePos[vi];
    }
    nrrdKernelSpecSet(vol->kspSS, kspSS->kernel, kspSS->parm);
  } else {
    vol->ninSingle = ninSingle;
    vol->ninScale = NULL;
    vol->scaleNum = 0;
    /* leave kspSS as is (unset) */
  }
  
  gageQueryReset(vol->gctx, vol->gpvl);
  /* the query is the single thing remaining unset in the gageContext */

  return 0;
}

/*
** the effect is to give pctx ownership of the vol
*/
int
pullVolumeSingleAdd(pullContext *pctx, 
                    char *name, const Nrrd *nin,
                    const gageKind *kind, 
                    const NrrdKernelSpec *ksp00,
                    const NrrdKernelSpec *ksp11,
                    const NrrdKernelSpec *ksp22) {
  char me[]="pullVolumeSingleSet", err[BIFF_STRLEN];
  pullVolume *vol;

  printf("!%s(%s): verbose %d\n", me, name, pctx->verbose);

  vol = pullVolumeNew();
  if (_pullVolumeSet(pctx, vol,
                     pctx->verbose, name,
                     nin,
                     NULL, NULL, 0, 
                     kind,
                     ksp00, ksp11, ksp22, NULL)) {
    sprintf(err, "%s: trouble", me);
    biffAdd(PULL, err); return 1;
  }

  /* add this volume to context */
  printf("!%s: adding pctx->vol[%u] = %p\n", me, pctx->volNum, vol);
  pctx->vol[pctx->volNum] = vol;
  pctx->volNum++;
  return 0;
}

/*
** the effect is to give pctx ownership of the vol
*/
int
pullVolumeStackAdd(pullContext *pctx,
                   char *name,
                   const Nrrd *nin,
                   const Nrrd *const *ninSS, double *scale,
                   unsigned int ninNum,
                   const gageKind *kind, 
                   const NrrdKernelSpec *ksp00,
                   const NrrdKernelSpec *ksp11,
                   const NrrdKernelSpec *ksp22,
                   const NrrdKernelSpec *kspSS) {
  char me[]="pullVolumeStackSet", err[BIFF_STRLEN];
  pullVolume *vol;

  vol = pullVolumeNew();
  if (_pullVolumeSet(pctx, vol, pctx->verbose, name,
                     nin, 
                     ninSS, scale, ninNum, 
                     kind, ksp00, ksp11, ksp22, kspSS)) {
    sprintf(err, "%s: trouble", me);
    biffAdd(PULL, err); return 1;
  }

  /* add this volume to context */
  pctx->vol[pctx->volNum++] = vol;
  return 0;
}

/*
** this is only used to create pullVolumes for the pullTasks
**
** DOES use biff
*/
pullVolume *
_pullVolumeCopy(const pullVolume *volOrig) {
  char me[]="pullVolumeCopy", err[BIFF_STRLEN];
  pullVolume *volNew;

  volNew = pullVolumeNew();
  if (_pullVolumeSet(NULL, volNew, volOrig->verbose, volOrig->name, 
                     volOrig->ninSingle,
                     volOrig->ninScale, volOrig->scalePos,
                     volOrig->scaleNum,
                     volOrig->gpvl->kind,
                     volOrig->ksp00, volOrig->ksp11,
                     volOrig->ksp22, volOrig->kspSS)) {
    sprintf(err, "%s: trouble creating new volume", me);
    biffAdd(PULL, err); return NULL;
  }
  volNew->seedOnly = volOrig->seedOnly;
  /* we just created a new (per-task) gageContext, and it will not learn
     the items from the info specs, so we have to add query here */
  if (gageQuerySet(volNew->gctx, volNew->gpvl, volOrig->gpvl->query)
      || gageUpdate(volNew->gctx)) {
    sprintf(err, "%s: trouble with new volume gctx", me);
    biffMove(PULL, err, GAGE); return NULL;
  }
  return volNew;
}

int
_pullInsideBBox(pullContext *pctx, double pos[4]) {

  return (AIR_IN_CL(pctx->bboxMin[0], pos[0], pctx->bboxMax[0]) &&
          AIR_IN_CL(pctx->bboxMin[1], pos[1], pctx->bboxMax[1]) &&
          AIR_IN_CL(pctx->bboxMin[2], pos[2], pctx->bboxMax[2]) &&
          AIR_IN_CL(pctx->bboxMin[3], pos[3], pctx->bboxMax[3]));
}

/*
** sets:
** pctx->haveScale
** pctx->bboxMin  ([0] through [3], always)
** pctx->bboxMax  (same)
*/
int
_pullVolumeSetup(pullContext *pctx) {
  char me[]="_pullVolumeSetup", err[BIFF_STRLEN];
  unsigned int ii;

  for (ii=0; ii<pctx->volNum; ii++) {
    if (gageUpdate(pctx->vol[ii]->gctx)) {
      sprintf(err, "%s: trouble setting up gage on vol %u/%u",
              me, ii, pctx->volNum);
      biffMove(PULL, err, GAGE); return 1;
    }
  }
  gageShapeBoundingBox(pctx->bboxMin, pctx->bboxMax,
                       pctx->vol[0]->gctx->shape);
  for (ii=1; ii<pctx->volNum; ii++) {
    double min[3], max[3];
    gageShapeBoundingBox(min, max, pctx->vol[ii]->gctx->shape);
    ELL_3V_MIN(pctx->bboxMin, pctx->bboxMin, min);
    ELL_3V_MIN(pctx->bboxMax, pctx->bboxMax, max);
  }
  /* have now computed bbox{Min,Max}[0,1,2]; now do bbox{Min,Max}[3] */
  pctx->bboxMin[3] = pctx->bboxMax[3] = 0.0;
  pctx->haveScale = AIR_FALSE;
  for (ii=0; ii<pctx->volNum; ii++) {
    if (pctx->vol[ii]->ninScale) {
      double sclMin, sclMax;
      sclMin = pctx->vol[ii]->scalePos[0];
      sclMax = pctx->vol[ii]->scalePos[pctx->vol[ii]->scaleNum-1];
      if (!pctx->haveScale) {
        pctx->bboxMin[3] = sclMin;
        pctx->bboxMax[3] = sclMax;
        pctx->haveScale = AIR_TRUE;
      } else {
        /* we already know haveScale; expand existing range */
        pctx->bboxMin[3] = AIR_MIN(sclMin, pctx->bboxMin[3]);
        pctx->bboxMax[3] = AIR_MAX(sclMax, pctx->bboxMax[3]);
      }
    }
  }
  printf("!%s: bbox min (%g,%g,%g,%g) max (%g,%g,%g,%g)\n", me,
         pctx->bboxMin[0], pctx->bboxMin[1],
         pctx->bboxMin[2], pctx->bboxMin[3],
         pctx->bboxMax[0], pctx->bboxMax[1],
         pctx->bboxMax[2], pctx->bboxMax[3]);

  /* _energyInterParticle() depends on this error checking */
  if (pctx->haveScale) {
    if (pullInterTypeJustR == pctx->interType) {
      sprintf(err, "%s: need scale-aware intertype (not %s) with "
              "a scale-space volume",
              me, airEnumStr(pullInterType, pullInterTypeJustR));
      biffAdd(PULL, err); return 1;
    }
  } else {
    /* don't have scale */
    if (pullInterTypeJustR != pctx->interType) {
      sprintf(err, "%s: can't use scale-aware intertype (%s) without "
              "a scale-space volume",
              me, airEnumStr(pullInterType, pctx->interType));
      biffAdd(PULL, err); return 1;
    }
  }

  return 0;
}

