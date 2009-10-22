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

#include "ten.h"
#include "privateTen.h"

static int
_experAlloc(tenExperSpec* espec, unsigned int num) {
  static char me[]="_experAlloc";
  
  espec->bval = airFree(espec->bval);
  espec->grad = airFree(espec->grad);
  espec->wght = airFree(espec->wght);
  if (!num) {
    biffAddf(TEN, "%s: need a non-zero number of images", me);
    return 1;
  }
  espec->bval = AIR_CAST(double *, calloc(num, sizeof(double)));
  espec->grad = AIR_CAST(double *, calloc(3*num, sizeof(double)));
  espec->wght = AIR_CAST(double *, calloc(num, sizeof(double)));
  if (!( espec->bval && espec->grad && espec->wght )) {
    biffAddf(TEN, "%s: couldn't allocate for %u images", me, num);
    return 1;
  }
  return 0;
}

tenExperSpec*
tenExperSpecNew(void) {
  tenExperSpec* espec;

  espec = AIR_CAST(tenExperSpec*, calloc(1, sizeof(tenExperSpec)));
  espec->set = AIR_FALSE;
  espec->imgNum = 0;
  espec->bval = NULL;
  espec->grad = NULL;
  espec->wght = NULL;
  return espec;
}

int
tenExperSpecGradSingleBValSet(tenExperSpec *espec,
                              unsigned int imgNum,
                              double bval,
                              const double *grad) {
  static const char me[]="tenExperSpecGradSingleBValSet";
  unsigned int ii;

  if (!espec) {
    biffAddf(TEN, "%s: got NULL pointer", me);
    return 1;
  }
  if (_experAlloc(espec, imgNum)) {
    biffAddf(TEN, "%s: couldn't allocate", me);
    return 1;
  }
  for (ii=0; ii<imgNum; ii++) {
    espec->bval[ii] = bval;
    ELL_3V_COPY(espec->grad + 3*ii, grad + 3*ii);
    espec->wght[ii] = 1.0;
  }

  return 0;
}

int
tenExperSpecGradBValSet(tenExperSpec *espec,
                        unsigned int imgNum,
                        const double *bval,
                        const double *grad) {
  static const char me[]="tenExperSpecGradBValSet";
  unsigned int ii;

  if (!espec) {
    biffAddf(TEN, "%s: got NULL pointer", me);
    return 1;
  }
  if (_experAlloc(espec, imgNum)) {
    biffAddf(TEN, "%s: couldn't allocate", me);
    return 1;
  }
  for (ii=0; ii<imgNum; ii++) {
    espec->bval[ii] = bval[ii];
    ELL_3V_COPY(espec->grad + 3*ii, grad + 3*ii);
    espec->wght[ii] = 1.0;
  }

  return 0;
}

int
tenExperSpecGradBValWghtSet(tenExperSpec *espec,
                            unsigned int imgNum,
                            const double *bval,
                            const double *grad,
                            const double *wght) {
  static const char me[]="tenExperSpecGradBValWghtSet";
  unsigned int ii;
  
  if (!espec) {
    biffAddf(TEN, "%s: got NULL pointer", me);
    return 1;
  }
  if (_experAlloc(espec, imgNum)) {
    biffAddf(TEN, "%s: couldn't allocate", me);
    return 1;
  }
  for (ii=0; ii<imgNum; ii++) {
    espec->bval[ii] = bval[ii];
    ELL_3V_COPY(espec->grad + 3*ii, grad + 3*ii);
    espec->wght[ii] = wght[ii];
  }

  return 0;
}

int
tenExperSpecFromKeyValueSet(tenExperSpec *espec, const Nrrd *ndwi) {
  static const char me[]="tenExperSpecFromKeyValueSet";
  unsigned int *skip, skipNum, ii, imgNum;
  Nrrd *ngrad, *nbmat;
  airArray *mop;
  double len, singleBval, *bval, *grad;

  if (tenDWMRIKeyValueParse(&ngrad, &nbmat, &singleBval,
                            &skip, &skipNum, ndwi)) {
    biffAddf(TEN, "%s: trouble parsing DWI info from key/value pairs", me);
    return 1;
  }
  mop = airMopNew();
  if (ngrad) {
    airMopAdd(mop, ngrad, (airMopper)nrrdNuke, airMopAlways);
  }
  if (nbmat) {
    airMopAdd(mop, nbmat, (airMopper)nrrdNuke, airMopAlways);
  }
  if (skip) {
    airMopAdd(mop, skip, airFree, airMopAlways);
  }

  if (nbmat) {
    biffAddf(TEN, "%s: sorry, currently can't handle B-matrices here", me);
    airMopError(mop); return 1;
  }
  if (skipNum) {
    biffAddf(TEN, "%s: sorry, currently can't handle skipping (%u) here", me,
             skipNum);
    airMopError(mop); return 1;
  }

  imgNum = ngrad->axis[1].size;
  bval = AIR_CAST(double *, calloc(imgNum, sizeof(double)));
  airMopAdd(mop, bval, airFree, airMopAlways);
  grad = AIR_CAST(double *, ngrad->data);
  for (ii=0; ii<imgNum; ii++) {
    len = ELL_3V_LEN(grad + 3*ii);
    bval[ii] = singleBval*len*len;
    if (len) {
      ELL_3V_SCALE(grad + 3*ii, 1/len, grad + 3*ii);
    } else {
      ELL_3V_SET(grad + 3*ii, 0, 0, -1);
    }
  }
  if (tenExperSpecGradBValSet(espec, imgNum, bval, grad)) {
    biffAddf(TEN, "%s: trouble", me);
    airMopError(mop); return 1;
  }
  airMopOkay(mop);
  return 0;
}

tenExperSpec*
tenExperSpecNix(tenExperSpec *espec) {

  if (espec) {
    espec->bval = airFree(espec->bval);
    espec->grad = airFree(espec->grad);
    espec->wght = airFree(espec->wght);
    airFree(espec);
  }
  return NULL;
}

double
_tenModel_sqe(const double *dwiMeas, const double *dwiSim,
              const tenExperSpec *espec) {
  unsigned int ii;
  double sqe;

  sqe = 0;
  for (ii=0; ii<espec->imgNum; ii++) {
    double dd;
    dd = dwiMeas[ii] - dwiSim[ii];
    sqe += dd*dd;
  }
  return sqe;
}

double
_tenModel_nll(const double *dwiMeas, const double *dwiSim,
              const tenExperSpec *espec,
              int rician, double sigma) {
  double nll;
  unsigned int ii;

  nll = 0;
  if (rician) {
    for (ii=0; ii<espec->imgNum; ii++) {
      nll += -airLogRician(dwiMeas[ii], dwiSim[ii], sigma);
    }
  } else {
    double dd, ladd, denom;
    ladd = log(sigma*sqrt(2*AIR_PI));
    denom = 1.0/(2*sigma*sigma);
    for (ii=0; ii<espec->imgNum; ii++) {
      dd = dwiMeas[ii] - dwiSim[ii];
      nll += dd*dd*denom + ladd;
    }
  }
  return nll;
}

int
tenDWMRIKeyValueFromExperSpecSet(Nrrd *ndwi, const tenExperSpec *espec) {
  static char me[]="tenDWMRIKeyValueFromExperSpecSet"; 
  char keystr[AIR_STRLEN_MED], valstr[AIR_STRLEN_MED];

  if (!(ndwi && espec)) {
    biffAddf(TEN, "%s: got NULL pointer", me);
    return 1;
  }

  AIR_UNUSED(keystr);
  AIR_UNUSED(valstr);

  /*
  nrrdKeyValueAdd(ndwi, tenDWMRIModalityKey, tenDWMRIModalityVal);
  sprintf(valstr, "%g", espec->);
    nrrdKeyValueAdd(ndwi, tenDWMRIBValueKey, valstr);
    if (tec->_ngrad) {
      lup = nrrdDLookup[tec->_ngrad->type];
      for (allIdx=0; allIdx<tec->allNum; allIdx++) {
        sprintf(keystr, tenDWMRIGradKeyFmt, allIdx);
        sprintf(valstr, "%g %g %g",
                lup(tec->_ngrad->data, 0 + 3*allIdx),
                lup(tec->_ngrad->data, 1 + 3*allIdx),
                lup(tec->_ngrad->data, 2 + 3*allIdx));
        nrrdKeyValueAdd(ndwi, keystr, valstr);
      }
    } else {
      lup = nrrdDLookup[tec->_nbmat->type];
      for (allIdx=0; allIdx<tec->allNum; allIdx++) {
        sprintf(keystr, tenDWMRIBmatKeyFmt, allIdx);
        sprintf(valstr, "%g %g %g %g %g %g",
                lup(tec->_nbmat->data, 0 + 6*allIdx),
                lup(tec->_nbmat->data, 1 + 6*allIdx),
                lup(tec->_nbmat->data, 2 + 6*allIdx),
                lup(tec->_nbmat->data, 3 + 6*allIdx),
                lup(tec->_nbmat->data, 4 + 6*allIdx),
                lup(tec->_nbmat->data, 5 + 6*allIdx));
        nrrdKeyValueAdd(ndwi, keystr, valstr);
      }
    }
  */

  return 0;
}

double
tenExperSpecKnownB0Get(const tenExperSpec *espec, const double *dwi) {
  unsigned int ii, nb;
  double b0; 
  
  if (!( dwi && espec )) {
    return AIR_NAN;
  }

  nb = 0;
  b0 = 0.0;
  for (ii=0; ii<espec->imgNum; ii++) {
    if (0 == espec->bval[ii]) {
      b0 += dwi[ii];
      ++nb;
    }
  }
  return b0/nb;
}
