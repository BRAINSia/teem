/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in
  compliance with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
  the License for the specific language governing rights and limitations
  under the License.

  The Original Source Code is "teem", released March 23, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1998 University
  of Utah. All Rights Reserved.
*/


#include <nrrd.h>

char *me; 

void
usage() {
  /*              0    1      2      3       4       (argc-1) */
  fprintf(stderr, 
	  "usage: %s <nin> <kern> <size0> <size1> ... <nout>\n\n", me);
  fprintf(stderr, "<kern> is the resampling kernel to use; options are:\n");
  fprintf(stderr, "  \"box\": box filter (nearest neighbor upsampling)\n");
  fprintf(stderr, "  \"tent\": tent filter (trilinear upsampling)\n");
  fprintf(stderr, "  \"cubic:B,C\": Mitchell/Netravali BC-family of piecewise cubics\n");
  fprintf(stderr, "     \"cubic:1.0,0.0\": B-spline w/ maximal blurring\n");
  fprintf(stderr, "     \"cubic:0.0,0.5\": Catmull-Rom kernel\n");
  fprintf(stderr, "    (\"cubic:0.0,C\": all interpolating cubics)\n");
  fprintf(stderr, "  \"quartic:A\": Gordon's family of interpolating quartics\n");
  fprintf(stderr, "     \"quartic:0.25\": most sinc()-like; use this!\n\n");
  fprintf(stderr, "<size0>, <size1>, ... are sizes along each axis of output\n");
  fprintf(stderr, "  These can be either less than or greater than original axis size\n");
  fprintf(stderr, "  size = \"x\": don't resampling along this axis at all\n");
  fprintf(stderr, "  For non-interpolating kernels, this is different than having\n");
  fprintf(stderr, "  old size == new size\n\n");
  exit(1);
}

int
main(int argc, char *argv[]) {
  char *in, *out, *err, *kernS, tkS[128], *sizeS;
  Nrrd *nin, *nout;
  nrrdResampleInfo *info;
  nrrdKernel *kern;
  float param[NRRD_MAX_KERNEL_PARAMS];
  int d, size;

  info = nrrdResampleInfoNew();
  
  me = argv[0];
  if (!(argc >= 5)) {
    usage();
  }
  in = argv[1];
  kernS = argv[2];
  out = argv[argc-1];
  if (!(nin = nrrdNewOpen(in))) {
    err = biffGet(NRRD);
    fprintf(stderr, "%s: trouble reading nrrd from \"%s\":%s\n", me, in, err);
    free(err);
    usage();
  }
  memset(param, 0, NRRD_MAX_KERNEL_PARAMS*sizeof(float));
  param[0] = 1.0;
  
  /* parse the kernel */
  strcpy(tkS, "box");
  if (!strcmp(tkS, kernS)) {
    kern = nrrdKernelBox;
    goto kparsed;
  }

  strcpy(tkS, "tent");
  if (!strcmp(tkS, kernS)) {
    kern = nrrdKernelTent;
    goto kparsed;
  }

  strcpy(tkS, "cubic");
  if (!strncmp(tkS, kernS, strlen(tkS))) {
    kern = nrrdKernelBCCubic;
    if (2 != sscanf(kernS+strlen(tkS), ":%f,%f", param+1, param+2)) {
      fprintf(stderr, "%s: couldn't parameters \"%s\" for %s kernel\n", 
	      me, kernS+strlen(tkS), tkS);
      usage();
    }
    goto kparsed;
  }

  strcpy(tkS, "quartic");
  if (!strncmp(tkS, kernS, strlen(tkS))) {
    kern = nrrdKernelAQuartic;
    if (1 != sscanf(kernS+strlen(tkS), ":%f", param+1)) {
      fprintf(stderr, "%s: couldn't parameters \"%s\" for %s kernel\n", 
	      me, kernS+strlen(tkS), tkS);
      usage();
    }
    goto kparsed;
  }

  fprintf(stderr, "%s: couldn't parse kernel \"%s\"\n", me, kernS);
  usage();
  
 kparsed:
  if (argc-4 != nin->dim) {
    fprintf(stderr, "%s: read in %d-D nrrd, but got %d resampling sizes\n",
	    me, nin->dim, argc-4);
    exit(1);
  }
  /* for the curious, the constraints/simplifications in this unrrdu,
     compared to the full options for resampler are:
     - always resampling full length of axis (min = 0, max = size-1)
       (no cropping, padding simultaneously with resampling) 
     - same kernel, with same parameters, for every axis
     - no rescaling of kernel (param[0] == 1.0)
     - boundary behavior is always bleed
     - output type is always input type
     - renormalization hack always on 
  */
  for (d=0; d<=nin->dim-1; d++) {
    sizeS = argv[3+d];
    if (!strcmp("x", sizeS)) {
      /* no resampling on this axis desired */
      info->kernel[d] = NULL;
    }
    else {
      if (1 != sscanf(sizeS, "%d", &size)) {
	fprintf(stderr, "%s: couldn't parse size %d \"%s\" as int\n", me,
		d, sizeS);
	usage();
      }
      if (!(size > 0)) {
	fprintf(stderr, "%s: invalid # samples (%d) for axis %d\n", 
		me, size, d);
	usage();
      }
      info->kernel[d] = kern;
      memcpy(info->param[d], param, NRRD_MAX_KERNEL_PARAMS*sizeof(float));
      info->min[d] = 0;
      info->max[d] = nin->size[d]-1;
      info->samples[d] = size;
    }
  }
  info->boundary = nrrdBoundaryBleed;
  info->type = nin->type;
  info->renormalize = AIR_TRUE;

  nout = nrrdNew();
  if (nrrdSpatialResample(nout, nin, info)) {
    err = biffGet(NRRD);
    fprintf(stderr, "%s: trouble resampling:\n%s\n", me, err);
    free(err);
    exit(1);
  }
  nout->encoding = nrrdEncodingRaw;
  if (nrrdSave(out, nout)) {
    fprintf(stderr, "%s: error writing nrrd:\n%s", me, biffGet(NRRD));
    exit(1);
  }
  nrrdNuke(nin);
  nrrdNuke(nout);
  nrrdResampleInfoNix(info);
  exit(0);
}
