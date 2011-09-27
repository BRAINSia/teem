/*
  Teem: Tools to process and visualize scientific data and images              
  Copyright (C) 2011, 2010, 2009  University of Chicago
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

#include "unrrdu.h"
#include "privateUnrrdu.h"

#define INFO "Ring removal for CT"
char *_unrrdu_deringInfoL =
(INFO
 ". (NOT FINISHED) ");

int
unrrdu_deringMain(int argc, const char **argv, char *me, hestParm *hparm) {
  hestOpt *opt = NULL;
  char *out, *err;
  Nrrd *nin, *nout;
  airArray *mop;
  int pret;

  NrrdDeringContext *drc;
  double center[2], radScale;
  int verbose, linterp;
  unsigned int thetaNum;

  hestOptAdd(&opt, "c,center", "x y", airTypeDouble, 2, 2, center, NULL,
             "center of rings, in index space of fastest two axes");
  hestOptAdd(&opt, "v,verbose", "v", airTypeInt, 1, 1, &verbose, "0",
             "verbosity level");
  hestOptAdd(&opt, "li,linterp", "bool", airTypeBool, 1, 1, &linterp, "false",
             "whether to use linear interpolation during polar transform");
  hestOptAdd(&opt, "tn,thetanum", "# smpls", airTypeUInt, 1, 1, &thetaNum,
             "20", "# of theta samples");
  hestOptAdd(&opt, "rs,radscale", "scale", airTypeDouble, 1, 1, &radScale,
             "1,0", "scaling on radius in polar transform");
  OPT_ADD_NIN(nin, "input nrrd");
  OPT_ADD_NOUT(out, "output nrrd");

  mop = airMopNew();
  airMopAdd(mop, opt, (airMopper)hestOptFree, airMopAlways);

  USAGE(_unrrdu_deringInfoL);
  PARSE();
  airMopAdd(mop, opt, (airMopper)hestParseFree, airMopAlways);
  
  nout = nrrdNew();
  airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);

  drc = nrrdDeringContextNew();
  airMopAdd(mop, drc, (airMopper)nrrdDeringContextNix, airMopAlways);

  if (nrrdDeringVerboseSet(drc, verbose)
      || nrrdDeringLinearInterpSet(drc, linterp)
      || nrrdDeringInputSet(drc, nin)
      || nrrdDeringCenterSet(drc, center[0], center[1])
      || nrrdDeringRadiusScaleSet(drc, radScale)
      || nrrdDeringThetaNumSet(drc, thetaNum)
      || nrrdDeringExecute(drc, nout)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error deringing:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

UNRRDU_CMD(dering, INFO);
