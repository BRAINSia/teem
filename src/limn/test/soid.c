/*
  teem: Gordon Kindlmann's research software
  Copyright (C) 2003, 2002, 2001, 2000, 1999, 1998 University of Utah

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


#include "../limn.h"

char *info = ("Render a single ellipsoid in postscript.");

void
washQtoM3(float m[9], float q[4]) {
  float p[4], w, x, y, z, len;

  ELL_4V_COPY(p, q);
  len = ELL_4V_LEN(p);
  ELL_4V_SCALE(p, 1.0/len, p);
  w = p[0];
  x = p[1];
  y = p[2];
  z = p[3];
  /* mathematica work implies that we should be 
     setting ROW vectors here */
  ELL_3V_SET(m+0, 
	     1 - 2*(y*y + z*z),
	     2*(x*y - w*z),
	     2*(x*z + w*y));
  ELL_3V_SET(m+3,
	     2*(x*y + w*z),
	     1 - 2*(x*x + z*z),
	     2*(y*z - w*x));
  ELL_3V_SET(m+6,
	     2*(x*z - w*y),
	     2*(y*z + w*x),
	     1 - 2*(x*x + y*y));
}

int
main(int argc, char *argv[]) {
  char *me, *err, *outS;
  limnCamera *cam;
  float p[3], q[4], mR[9], eval[3], len, sh, cl, cp, qA, qB;
  float matA[16], matB[16], os, sc[3], rad, edgeWidth[5];
  hestOpt *hopt=NULL;
  airArray *mop;
  limnObj *obj;
  limnSP *sp;
  limnPart *r;
  limnWin *win;
  int ri, si, res, axis, sphere;
  Nrrd *nmap;

  mop = airMopNew();
  cam = limnCameraNew();
  airMopAdd(mop, cam, (airMopper)limnCameraNix, airMopAlways);
  
  edgeWidth[0] = 0;
  edgeWidth[1] = 0;
  me = argv[0];
  hestOptAdd(&hopt, "fr", "from point", airTypeDouble, 3, 3, cam->from,"4 4 4",
	     "position of camera, used to determine view vector");
  hestOptAdd(&hopt, "at", "at point", airTypeDouble, 3, 3, cam->at, "0 0 0",
	     "camera look-at point, used to determine view vector");
  hestOptAdd(&hopt, "up", "up vector", airTypeDouble, 3, 3, cam->up, "0 0 1",
	     "camera pseudo-up vector, used to determine view coordinates");
  hestOptAdd(&hopt, "rh", NULL, airTypeInt, 0, 0, &(cam->rightHanded), NULL,
	     "use a right-handed UVN frame (V points down)");
  hestOptAdd(&hopt, "or", NULL, airTypeInt, 0, 0, &(cam->orthographic), NULL,
	     "use orthogonal projection");
  hestOptAdd(&hopt, "ur", "uMin uMax", airTypeDouble, 2, 2, cam->uRange,
	     "-1 1", "range in U direction of image plane");
  hestOptAdd(&hopt, "vr", "vMin vMax", airTypeDouble, 2, 2, cam->vRange,
	     "-1 1", "range in V direction of image plane");
  hestOptAdd(&hopt, "e", "envmap", airTypeOther, 1, 1, &nmap, NULL,
	     "16checker-based environment map",
	     NULL, NULL, nrrdHestNrrd);
  hestOptAdd(&hopt, "sc", "scalings", airTypeFloat, 3, 3, sc, "1 1 1",
	     "axis-aligned scaling to do on ellipsoid");
  hestOptAdd(&hopt, "os", "over-all scaling", airTypeFloat, 1, 1, &os, "1",
	     "over-all scaling (multiplied by scalings)");
  hestOptAdd(&hopt, "sh", "superquad sharpness", airTypeFloat, 1, 1, &sh, "0",
	     "how much to sharpen edges as a "
	     "function of differences between eigenvalues");
  hestOptAdd(&hopt, "sphere", NULL, airTypeInt, 0, 0, &sphere, NULL,
	     "none of this superquadric crap: use a sphere, dammit");
  hestOptAdd(&hopt, "p", "x y z", airTypeFloat, 3, 3, p, "0 0 0",
	     "location in quaternion quotient space");
  hestOptAdd(&hopt, "r", "radius", airTypeFloat, 1, 1, &rad, "0.015",
	     "black axis cylinder radius (or 0.0 to not drawn these)");
  hestOptAdd(&hopt, "res", "resolution", airTypeInt, 1, 1, &res, "25",
	     "tesselation resolution for both glyph and axis cylinders");
  hestOptAdd(&hopt, "wd", "3 widths", airTypeFloat, 3, 3, edgeWidth + 2,
	     "1.5 0.7 0.0",
	     "width of edges drawn for three kinds of "
	     "edges: silohuette, crease, non-crease");
  hestOptAdd(&hopt, "o", "output PS", airTypeString, 1, 1, &outS, "out.ps",
	     "output file to render postscript into");
  hestParseOrDie(hopt, argc-1, argv+1, NULL,
		 me, info, AIR_TRUE, AIR_TRUE, AIR_TRUE);
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);
  
  cam->neer = -0.000000001;
  cam->dist = 0;
  cam->faar = 0.0000000001;
  cam->atRelative = AIR_TRUE;

  if (limnCameraUpdate(cam)) {
    fprintf(stderr, "%s: trouble:\n%s\n", me, err = biffGet(LIMN));
    free(err);
    return 1;
  }
  obj = limnObjNew(10, AIR_TRUE);
  airMopAdd(mop, obj, (airMopper)limnObjNix, airMopAlways);

  /* create limnSPs for ellipsoid (#0) and for rods (#1) */
  si = airArrayIncrLen(obj->sA, 2);
  sp = obj->s + si + 0;
  ELL_4V_SET(sp->rgba, 1, 1, 1, 1);  /* this is kind of silly */
  ELL_3V_SET(sp->k, 0.2, 0.8, 0);
  sp->spec = 0;
  sp = obj->s + si + 1;
  ELL_4V_SET(sp->rgba, 1, 1, 1, 1);  /* this is kind of silly */
  ELL_3V_SET(sp->k, 1, 0, 0);
  sp->spec = 0;

  q[0] = 1.0;
  q[1] = p[0];
  q[2] = p[1];
  q[3] = p[2];
  len = ELL_4V_LEN(q);
  ELL_4V_SCALE(q, 1.0/len, q);
  washQtoM3(mR, q);

  sc[0] *= os; sc[1] *= os; sc[2] *= os;
  ELL_3V_COPY(eval, sc);
  ELL_SORT3(eval[0], eval[1], eval[2], cl);
  cl = (eval[0] - eval[1])/(eval[0] + eval[1] + eval[2]);
  cp = 2*(eval[1] - eval[2])/(eval[0] + eval[1] + eval[2]);
  if (cl > cp) {
    axis = ELL_MAX3_IDX(sc[0], sc[1], sc[2]);
    qA = pow(1-cp, sh);
    qB = pow(1-cl, sh);
  } else {
    axis = ELL_MIN3_IDX(sc[0], sc[1], sc[2]);
    qA = pow(1-cl, sh);
    qB = pow(1-cp, sh);
  }
  fprintf(stderr, "eval = %g %g %g -> cl=%g %s cp=%g -> axis = %d\n",
	  eval[0], eval[1], eval[2], cl, cl > cp ? ">" : "<", cp, axis);

  if (sphere) {
    ri = limnObjPolarSphereAdd(obj, 0, 0, 2*res, res);
  } else {
    ri = limnObjPolarSuperquadAdd(obj, 0, axis, qA, qB, 2*res, res);
  }
  r = obj->r + ri; ELL_4V_SET(r->rgba, 1, 1, 1, 1);
  ELL_4M_IDENTITY_SET(matA);
  ELL_4M_SCALE_SET(matB, sc[0], sc[1], sc[2]);
  ell_4m_post_mul_f(matA, matB);
  ELL_43M_INSET(matB, mR);
  ell_4m_post_mul_f(matA, matB);
  limnObjPartTransform(obj, ri, matA);

  if (rad) {
    ri = limnObjCylinderAdd(obj, 1, 0, res);
    r = obj->r + ri; ELL_4V_SET(r->rgba, 0, 0, 0, 1);
    ELL_4M_IDENTITY_SET(matA);
    ELL_4M_SCALE_SET(matB, (1-sc[0])/2, rad, rad);
    ell_4m_post_mul_f(matA, matB);
    ELL_4M_TRANSLATE_SET(matB, (1+sc[0])/2, 0.0, 0.0); 
    ell_4m_post_mul_f(matA, matB);
    limnObjPartTransform(obj, ri, matA);
    
    ri = limnObjCylinderAdd(obj, 1, 0, res);
    r = obj->r + ri; ELL_4V_SET(r->rgba, 0, 0, 0, 1);
    ELL_4M_IDENTITY_SET(matA);
    ELL_4M_SCALE_SET(matB, (1-sc[0])/2, rad, rad);
    ell_4m_post_mul_f(matA, matB);
    ELL_4M_TRANSLATE_SET(matB, -(1+sc[0])/2, 0.0, 0.0); 
    ell_4m_post_mul_f(matA, matB);
    limnObjPartTransform(obj, ri, matA);
    
    
    ri = limnObjCylinderAdd(obj, 1, 1, res);
    r = obj->r + ri; ELL_4V_SET(r->rgba, 0, 0, 0, 1);
    ELL_4M_IDENTITY_SET(matA);
    ELL_4M_SCALE_SET(matB, rad, (1-sc[1])/2, rad);
    ell_4m_post_mul_f(matA, matB);
    ELL_4M_TRANSLATE_SET(matB, 0.0, (1+sc[1])/2, 0.0); 
    ell_4m_post_mul_f(matA, matB);
    limnObjPartTransform(obj, ri, matA);
    
    ri = limnObjCylinderAdd(obj, 1, 1, res);
    r = obj->r + ri; ELL_4V_SET(r->rgba, 0, 0, 0, 1);
    ELL_4M_IDENTITY_SET(matA);
    ELL_4M_SCALE_SET(matB, rad, (1-sc[1])/2, rad);
    ell_4m_post_mul_f(matA, matB);
    ELL_4M_TRANSLATE_SET(matB, 0.0, -(1+sc[1])/2, 0.0); 
    ell_4m_post_mul_f(matA, matB);
    limnObjPartTransform(obj, ri, matA);
    
    
    ri = limnObjCylinderAdd(obj, 1, 2, res);
    r = obj->r + ri; ELL_4V_SET(r->rgba, 0, 0, 0, 1);
    ELL_4M_IDENTITY_SET(matA);
    ELL_4M_SCALE_SET(matB, rad, rad, (1-sc[2])/2);
    ell_4m_post_mul_f(matA, matB);
    ELL_4M_TRANSLATE_SET(matB, 0.0, 0.0, (1+sc[2])/2); 
    ell_4m_post_mul_f(matA, matB);
    limnObjPartTransform(obj, ri, matA);
    
    ri = limnObjCylinderAdd(obj, 1, 2, res);
    r = obj->r + ri; ELL_4V_SET(r->rgba, 0, 0, 0, 1);
    ELL_4M_IDENTITY_SET(matA);
    ELL_4M_SCALE_SET(matB, rad, rad, (1-sc[2])/2);
    ell_4m_post_mul_f(matA, matB);
    ELL_4M_TRANSLATE_SET(matB, 0.0, 0.0, -(1+sc[2])/2); 
    ell_4m_post_mul_f(matA, matB);
    limnObjPartTransform(obj, ri, matA);
  }

  win = limnWinNew(limnDevicePS);
  ELL_5V_COPY(win->ps.edgeWidth, edgeWidth);
  win->scale = 200;

  win->file = fopen(outS, "w");
  airMopAdd(mop, win, (airMopper)limnWinNix, airMopAlways);

  if (limnObjRender(obj, cam, win)
      || limnObjPSDraw(obj, cam, nmap, win)) {
    airMopAdd(mop, err = biffGetDone(LIMN), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s\n", me, err);
    airMopError(mop); return 1;
  }
  fclose(win->file);
  
  airMopOkay(mop);
  return 0;
}

