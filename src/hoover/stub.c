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

#include "hoover.h"

int
hoovStubRenderBegin(void **rendInfoP, void *userInfo) {
  
  return 0;
}

int
hoovStubThreadBegin(void **threadInfoP, void *rendInfo, void *userInfo,
		    int whichThread) {

  return 0;
}

int 
hoovStubRayBegin(void *threadInfo, void *rendInfo, void *userInfo,
		 int uIndex, int vIndex, 
		 double dirWorld[3], double dirIndex[3]) {
  return 0;
}

double
hoovStubSample(void *threadInfo, void *rendInfo, void *userInfo,
	       int in, double pos[3]) {
  return 0;
}

int
hoovStubRayEnd(void *threadInfo, void *rendInfo, void *userInfo) {

  return 0;
}

int
hoovStubThreadEnd(void *threadInfo, void *rendInfo, void *userInfo) {

  return 0;
}

int
hoovStubRenderEnd(void *rendInfo, void *userInfo) {

  return 0;
}

