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

#include "ten.h"
#include "tenPrivate.h"

#define INFO "Calculate one or more eigenvalues in a DT volume"
char *_tend_evalInfoL =
  (INFO
   ". ");

int
tend_evalMain(int argc, char **argv, char *me, hestParm *hparm) {
  int pret, map[4];
  hestOpt *hopt = NULL;
  char *perr, *err;
  airArray *mop;

  int ret, *comp, compLen, cc, sx, sy, sz;
  Nrrd *nin, *nout;
  char *outS;
  float thresh, *edata, *tdata, eval[3], evec[9];
  size_t N, I;

  hestOptAdd(&hopt, "c", "c0 ", airTypeInt, 1, 3, &comp, NULL,
	     "which eigenvalues should be saved out. \"0\" for the "
	     "largest, \"1\" for the middle, \"2\" for the smallest, "
	     "\"0 1\", \"1 2\", \"0 1 2\" or similar for more than one",
	     &compLen);
  hestOptAdd(&hopt, "t", "thresh", airTypeFloat, 1, 1, &thresh, "0.5",
	     "confidence threshold");
  hestOptAdd(&hopt, "i", "nin", airTypeOther, 1, 1, &nin, "-",
	     "input diffusion tensor volume", NULL, NULL, nrrdHestNrrd);
  hestOptAdd(&hopt, "o", "nout", airTypeString, 1, 1, &outS, "-",
	     "output image (floating point)");

  mop = airMopNew();
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  USAGE(_tend_evalInfoL);
  PARSE();
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);

  for (cc=0; cc<compLen; cc++) {
    if (!AIR_IN_CL(0, comp[cc], 2)) {
      fprintf(stderr, "%s: requested component %d (%d of 3) not in [0..2]\n",
	      me, comp[cc], cc+1);
      airMopError(mop); return 1;
    }
  }
  if (tenTensorCheck(nin, nrrdTypeFloat, AIR_TRUE)) {
    airMopAdd(mop, err=biffGetDone(TEN), airFree, airMopAlways);
    fprintf(stderr, "%s: didn't get a valid DT volume:\n%s\n", me, err);
    airMopError(mop); return 1;
  }
  
  sx = nin->axis[1].size;
  sy = nin->axis[2].size;
  sz = nin->axis[3].size;

  nout = nrrdNew();
  airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
  if (1 == compLen) {
    ret = nrrdMaybeAlloc(nout, nrrdTypeFloat, 3, sx, sy, sz);
  } else {
    ret = nrrdMaybeAlloc(nout, nrrdTypeFloat, 4, compLen, sx, sy, sz);
  }
  if (ret) {
    airMopAdd(mop, err=biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble allocating output:\n%s\n", me, err);
    airMopError(mop); return 1;
  }

  N = sx*sy*sz;
  edata = nout->data;
  tdata = nin->data;
  if (1 == compLen) {
    ELL_3V_SET(map, 1, 2, 3);
    for (I=0; I<N; I++) {
      tenEigensolve(eval, evec, tdata);
      edata[I] = (tdata[0] >= thresh)*eval[comp[0]];
      tdata += 7;
    }
  } else {
    ELL_4V_SET(map, 0, 1, 2, 3);
    for (I=0; I<N; I++) {
      tenEigensolve(eval, evec, tdata);
      for (cc=0; cc<compLen; cc++)
	edata[cc] = (tdata[0] >= thresh)*eval[comp[cc]];
      edata += compLen;
      tdata += 7;
    }
  }
  if (nrrdAxesCopy(nout, nin, map, NRRD_AXESINFO_SIZE_BIT)) {
    airMopAdd(mop, err=biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s\n", me, err);
    airMopError(mop); return 1;
  }
  if (1 != compLen) {
    AIR_FREE(nout->axis[0].label);
  }

  if (nrrdSave(outS, nout, NULL)) {
    airMopAdd(mop, err=biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble writing:\n%s\n", me, err);
    airMopError(mop); return 1;
  }

  airMopOkay(mop);
  return 0;
}
TEND_CMD(eval, INFO);
