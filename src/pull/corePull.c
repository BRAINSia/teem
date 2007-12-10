/*
  Teem: Tools to process and visualize scientific data and images
  Copyright (C) 2006, 2005  Gordon Kindlmann
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

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "pull.h"
#include "privatePull.h"

/*
** this is the core of the worker threads: as long as there are bins
** left to process, get the next one, and process it
*/
int
_pullProcess(pullTask *task) {
  char me[]="_pullProcess", err[BIFF_STRLEN];
  unsigned int binIdx;
  
  while (task->pctx->binIdx < task->pctx->binNum) {
    /* get the index of the next bin to process */
    if (task->pctx->threadNum > 1) {
      airThreadMutexLock(task->pctx->binMutex);
    }
    do {
      binIdx = task->pctx->binIdx;
      if (task->pctx->binIdx < task->pctx->binNum) {
        task->pctx->binIdx++;
      }
    } while (binIdx < task->pctx->binNum
             && 0 == task->pctx->bin[binIdx].pointNum);
    if (task->pctx->threadNum > 1) {
      airThreadMutexUnlock(task->pctx->binMutex);
    }
    if (binIdx == task->pctx->binNum) {
      /* no more bins to process! */
      break;
    }
    if (pullBinProcess(task, binIdx)) {
      sprintf(err, "%s(%u): had trouble on bin %u", me,
              task->threadIdx, binIdx);
      biffAdd(PULL, err); return 1;
    }

  }
  return 0;
}

/* the main loop for each worker thread */
void *
_pullWorker(void *_task) {
  char me[]="_pushWorker", err[BIFF_STRLEN];
  pullTask *task;
  
  task = (pullTask *)_task;

  while (1) {
    if (task->pctx->verbose > 1) {
      fprintf(stderr, "%s(%u): waiting on barrier A\n",
              me, task->threadIdx);
    }
    /* pushFinish sets finished prior to the barriers */
    airThreadBarrierWait(task->pctx->iterBarrierA);
    if (task->pctx->finished) {
      if (task->pctx->verbose > 1) {
        fprintf(stderr, "%s(%u): done!\n", me, task->threadIdx);
      }
      break;
    }
    /* else there's work to do ... */    
    if (task->pctx->verbose > 1) {
      fprintf(stderr, "%s(%u): starting to process\n", me, task->threadIdx);
    }
    if (_pullProcess(task)) {
      /* HEY clearly not threadsafe ... */
      sprintf(err, "%s: thread %u trouble", me, task->threadIdx);
      biffAdd(PULL, err); 
      task->pctx->finished = AIR_TRUE;
    }
    if (task->pctx->verbose > 1) {
      fprintf(stderr, "%s(%u): waiting on barrier B\n",
              me, task->threadIdx);
    }
    airThreadBarrierWait(task->pctx->iterBarrierB);
  }

  return _task;
}

int
_pullContextCheck(pullContext *pctx) {
  char me[]="_pullContextCheck", err[BIFF_STRLEN];
  unsigned int ii;
  int gotIspec;

  if (!pctx) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(PULL, err); return 1;
  }
  if (pctx->npos) {
    if (nrrdCheck(pctx->npos)) {
      sprintf(err, "%s: got a broken npos", me);
      biffMove(PULL, err, NRRD); return 1;
    }
    if (!( 2 == pctx->npos->dim 
           && 4 == pctx->npos->axis[0].size
           && nrrdTypeDouble == pctx->npos->type )) {
      sprintf(err, "%s: npos not a 2-D 4-by-N array of %s "
              "(got %u-D %u-by-X of %s)", me,
              airEnumStr(nrrdType, nrrdTypeDouble),
              pctx->npos->dim,
              AIR_CAST(unsigned int, pctx->npos->axis[0].size),
              airEnumStr(nrrdType, pctx->npos->type));
      biffAdd(PULL, err); return 1;
    }
  } else {
    if (!( pctx->pointNum >= 1 )) {
      sprintf(err, "%s: pctx->pointNum (%d) not >= 1\n", me, pctx->pointNum);
      biffAdd(PULL, err); return 1;
    }
  }
  if (!pctx->volNum) {
    sprintf(err, "%s: have no volumes set", me);
    biffAdd(PULL, err); return 1;
  }
  gotIspec = AIR_FALSE;
  for (ii=0; ii<=PULL_INFO_MAX; ii++) {
    if (pctx->ispec[ii]) {
      /* make sure we have extra info as necessary */
      switch (ii) {
      case pullInfoInside:
      case pullInfoHeight:
      case pullInfoIsosurfaceValue:
        if (!( AIR_EXISTS(pctx->ispec[ii]->scaling)
               && AIR_EXISTS(pctx->ispec[ii]->zero) )) {
          sprintf(err, "%s: %s info needs scaling (%g) and zero (%g)", me, 
                  airEnumStr(pullInfo, ii),
                  pctx->ispec[ii]->scaling, pctx->ispec[ii]->zero);
          biffAdd(PULL, err); return 1;
        }
        break;
      }
      gotIspec = AIR_TRUE;
    }
  }
  if (!gotIspec) {
    sprintf(err, "%s: have no infos set", me);
    biffAdd(PULL, err); return 1;
  }
  if (pctx->ispec[pullInfoInside]) {
    if (!pctx->ispec[pullInfoInsideGradient]) {
      sprintf(err, "%s: want %s but don't have %s set", me, 
              airEnumStr(pullInfo, pullInfoInside),
              airEnumStr(pullInfo, pullInfoInsideGradient));
      biffAdd(PULL, err); return 1;
    }
  }
  if (pctx->ispec[pullInfoHeight]) {
    if (!( pctx->ispec[pullInfoHeightGradient]
           && pctx->ispec[pullInfoHeightHessian] )) {
      sprintf(err, "%s: want %s but don't have %s and %s set", me, 
              airEnumStr(pullInfo, pullInfoHeight),
              airEnumStr(pullInfo, pullInfoHeightGradient),
              airEnumStr(pullInfo, pullInfoHeightHessian));
      biffAdd(PULL, err); return 1;
    }
  }
  if (pctx->ispec[pullInfoIsosurfaceValue]) {
    if (!( pctx->ispec[pullInfoIsosurfaceGradient]
           && pctx->ispec[pullInfoIsosurfaceHessian] )) {
      sprintf(err, "%s: want %s but don't have %s and %s set", me, 
              airEnumStr(pullInfo, pullInfoIsosurfaceValue),
              airEnumStr(pullInfo, pullInfoIsosurfaceGradient),
              airEnumStr(pullInfo, pullInfoIsosurfaceHessian));
      biffAdd(PULL, err); return 1;
    }
  }
  if (!( AIR_IN_CL(1, pctx->threadNum, PULL_THREAD_MAXNUM) )) {
    sprintf(err, "%s: pctx->threadNum (%d) outside valid range [1,%d]", me,
            pctx->threadNum, PULL_THREAD_MAXNUM);
    biffAdd(PULL, err); return 1;
  }

  return 0;
}

int
pullStart(pullContext *pctx) {
  char me[]="pullStart", err[BIFF_STRLEN];
  unsigned int tidx;

  if (_pullContextCheck(pctx)) {
    sprintf(err, "%s: trouble", me);
    biffAdd(PULL, err); return 1;
  }

  airSrandMT(pctx->seedRNG);
  
  /* the ordering of steps below is important: gage context
     has to be set up before its copied by task setup */
  pctx->step = pctx->stepInitial;
  if (_pullInfoSetup(pctx) 
      || _pullTaskSetup(pctx)
      || _pullBinSetup(pctx)
      || _pullPointSetup(pctx)) {
    sprintf(err, "%s: trouble setting up context", me);
    biffAdd(PULL, err); return 1;
  }
  fprintf(stderr, "!%s: setup done-ish\n", me);

  if (pctx->threadNum > 1) {
    pctx->binMutex = airThreadMutexNew();
    pctx->iterBarrierA = airThreadBarrierNew(pctx->threadNum);
    pctx->iterBarrierB = airThreadBarrierNew(pctx->threadNum);
    /* start threads 1 and up running; they'll all hit iterBarrierA  */
    for (tidx=1; tidx<pctx->threadNum; tidx++) {
      if (pctx->verbose > 1) {
        fprintf(stderr, "%s: spawning thread %d\n", me, tidx);
      }
      airThreadStart(pctx->task[tidx]->thread, _pullWorker,
                     (void *)(pctx->task[tidx]));
    }
  } else {
    pctx->binMutex = NULL;
    pctx->iterBarrierA = NULL;
    pctx->iterBarrierB = NULL;
  }
  pctx->iter = 0;

  return 0;
}

/*
******** pullIterate
**
** (documentation)
**
** NB: this implements the body of thread 0, the master thread
*/
int
pullIterate(pullContext *pctx) {
  char me[]="pullIterate", err[BIFF_STRLEN];
  unsigned int ti, pointNum;
  double time0, time1;
  int myError;

  if (!pctx) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(PULL, err); return 1;
  }
  
  if (pctx->verbose) {
    fprintf(stderr, "%s: starting iterations\n", me);
  }

  time0 = airTime();

  /* the _pullWorker checks finished after iterBarrierA */
  pctx->finished = AIR_FALSE;
  pctx->binIdx=0;
  for (ti=0; ti<pctx->threadNum; ti++) {
    pctx->task[ti]->pointNum = 0;
    pctx->task[ti]->energySum = 0;
    pctx->task[ti]->deltaFracSum = 0;
  }

  if (pctx->verbose) {
    fprintf(stderr, "%s: starting iter %d w/ %u threads\n",
            me, pctx->iter, pctx->threadNum);
  }
  if (pctx->threadNum > 1) {
    airThreadBarrierWait(pctx->iterBarrierA);
  }
  myError = AIR_FALSE;
  if (_pullProcess(pctx->task[0])) {
    sprintf(err, "%s: master thread trouble w/ iter %u", me, pctx->iter);
    biffAdd(PULL, err);
    pctx->finished = AIR_TRUE;
    myError = AIR_TRUE;
  }
  if (pctx->threadNum > 1) {
    airThreadBarrierWait(pctx->iterBarrierB);
  }
  if (pctx->finished) {
    if (!myError) {
      /* we didn't set finished- one of the workers must have */
      sprintf(err, "%s: worker error on iter %u", me, pctx->iter);
      biffAdd(PULL, err); 
    }
    return 1;
  }

  pctx->energySum = 0;
  pctx->deltaFrac = 0;
  pointNum = 0;
  for (ti=0; ti<pctx->threadNum; ti++) {
    pctx->energySum += pctx->task[ti]->energySum;
    pctx->deltaFrac += pctx->task[ti]->deltaFracSum;
    pointNum += pctx->task[ti]->pointNum;
  }
  pctx->deltaFrac /= pointNum;
  if (pullRebin(pctx)) {
    sprintf(err, "%s: problem with new point locations", me);
    biffAdd(PULL, err); return 1;
  }

  time1 = airTime();
  pctx->timeIteration = time1 - time0;
  pctx->timeRun += time1 - time0;
  pctx->iter += 1;

  return 0;
}

int
pullRun(pullContext *pctx) {
  char me[]="pullRun", err[BIFF_STRLEN],
    poutS[AIR_STRLEN_MED];
  Nrrd *npos;
  double time0, time1, enrLast,
    enrNew=AIR_NAN, enrImprov=AIR_NAN, enrImprovAvg=AIR_NAN;
  
  if (pullIterate(pctx)) {
    sprintf(err, "%s: trouble on starting iteration", me);
    biffAdd(PULL, err); return 1;
  }
  fprintf(stderr, "!%s: starting pctx->energySum = %g\n",
          me, pctx->energySum);

  time0 = airTime();
  pctx->iter = 0;
  do {
    enrLast = pctx->energySum;
    if (pullIterate(pctx)) {
      sprintf(err, "%s: trouble on iter %d", me, pctx->iter);
      biffAdd(PULL, err); return 1;
    }
    if (pctx->snap && !(pctx->iter % pctx->snap)) {
      npos = nrrdNew();
      sprintf(poutS, "snap.%06d.pos.nrrd", pctx->iter);
      if (pullOutputGet(npos, NULL, pctx)) {
        sprintf(err, "%s: couldn't get snapshot for iter %d", me, pctx->iter);
        biffAdd(PULL, err); return 1;
      }
      if (nrrdSave(poutS, npos, NULL)) {
        sprintf(err, "%s: couldn't save snapshot for iter %d", me, pctx->iter);
        biffMove(PULL, err, NRRD); return 1;
      }
      npos = nrrdNuke(npos);
    }
    enrNew = pctx->energySum;
    enrImprov = 2*(enrLast - enrNew)/(enrLast + enrNew);
    fprintf(stderr, "!%s: %u, e=%g, de=%g,%g, df=%g\n",
            me, pctx->iter, enrNew, enrImprov, enrImprovAvg, pctx->deltaFrac);
    if (enrImprov < 0 || pctx->deltaFrac < pctx->deltaFracMin) {
      /* either energy went up instead of down,
         or particles were hitting their speed limit too much */
      double tmp;
      tmp = pctx->step;
      if (enrImprov < 0) {
        pctx->step *= pctx->energyStepFrac;
        fprintf(stderr, "%s: ***** iter %u e improv = %g; step = %g --> %g\n",
                me, pctx->iter, enrImprov, tmp, pctx->step);
      } else {
        pctx->step *= pctx->deltaFracStepFrac;
        fprintf(stderr, "%s: ##### iter %u deltaf = %g; step = %g --> %g\n",
                me, pctx->iter, pctx->deltaFrac, tmp, pctx->step);
      }
      /* this forces another iteration */
      enrImprovAvg = AIR_NAN; 
    } else {
      /* there was some improvement; energy went down */
      if (!AIR_EXISTS(enrImprovAvg)) {
        /* either enrImprovAvg has initial NaN setting, or was set to NaN
           because we had to decrease step size; either way we now
           re-initialize it to a large-ish value, to delay convergence */
        enrImprovAvg = 3*enrImprov;
      } else {
        /* we had improvement this iteration and last, do weighted average
           of the two, so that we are measuring the trend, rather than being
           sensitive to two iterations that just happen to have the same
           energy.  Thus, when enrImprovAvg gets near user-defined threshold,
           we really must have converged */
        enrImprovAvg = (enrImprovAvg + enrImprov)/2;
      }
    }
  } while ( ((!AIR_EXISTS(enrImprovAvg) 
              || enrImprovAvg > pctx->energyImprovMin)
             && (0 == pctx->maxIter
                 || pctx->iter < pctx->maxIter)) );
  fprintf(stderr, "%s: done after %u iters; enr = %g, enrImprov = %g,%g\n", 
          me, pctx->iter, enrNew, enrImprov, enrImprovAvg);
  time1 = airTime();

  pctx->timeRun = time1 - time0;

  return 0;
}

/*
** this is called *after* pullOutputGet
**
** should nix everything created by the many _pull*Setup() functions
*/
int
pullFinish(pullContext *pctx) {
  char me[]="pullFinish", err[BIFF_STRLEN];
  unsigned int ii, tidx;

  if (!pctx) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(PULL, err); return 1;
  }

  pctx->finished = AIR_TRUE;
  if (pctx->threadNum > 1) {
    if (pctx->verbose > 1) {
      fprintf(stderr, "%s: finishing workers\n", me);
    }
    airThreadBarrierWait(pctx->iterBarrierA);
  }
  /* worker threads now pass barrierA and see that finished is AIR_TRUE,
     and then bail, so now we collect them */
  for (tidx=pctx->threadNum; tidx>0; tidx--) {
    if (tidx-1) {
      airThreadJoin(pctx->task[tidx-1]->thread,
                    &(pctx->task[tidx-1]->returnPtr));
    }
    pctx->task[tidx-1]->thread = airThreadNix(pctx->task[tidx-1]->thread);
    pctx->task[tidx-1] = _pullTaskNix(pctx->task[tidx-1]);
  }
  pctx->task = (pullTask **)airFree(pctx->task);

  for (ii=0; ii<pctx->binNum; ii++) {
    pullBinDone(pctx->bin + ii);
  }
  pctx->bin = (pullBin *)airFree(pctx->bin);
  ELL_3V_SET(pctx->binsEdge, 0, 0, 0);
  pctx->binNum = 0;

  if (pctx->threadNum > 1) {
    pctx->binMutex = airThreadMutexNix(pctx->binMutex);
    pctx->iterBarrierA = airThreadBarrierNix(pctx->iterBarrierA);
    pctx->iterBarrierB = airThreadBarrierNix(pctx->iterBarrierB);
  }

  return 0;
}
