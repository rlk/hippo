// Copyright (C) 2005-2013 Robert Kooima
//
// This file is part of Hippo.
//
// Hippo is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// Hippo is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along
// with Hippo. If not, see <http://www.gnu.org/licenses/>.

#ifndef HIPPO_H
#define HIPPO_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------

struct star
{
    float pos[3];
    float mag[2];
};

typedef struct star  star;
typedef struct hippo hippo;

//-----------------------------------------------------------------------------

typedef void (*hippo_seek_fn)(const star *v, uint32_t c);

hippo      *hippo_read    (const char *filename);
hippo      *hippo_read_hip(const char *filename, uint32_t d);
hippo      *hippo_read_tyc(const char *filename, uint32_t d);

void        hippo_free (hippo *H);
int         hippo_write(hippo *H, const char *filename);

void        hippo_seek(const hippo *H, const float *v, int c, hippo_seek_fn fn);
const star *hippo_data(const hippo *H);
uint32_t    hippo_size(const hippo *H);

void        hippo_view_bound(float *v, const float *M);
void        hippo_cube_bound(float *v, const float *p, float d);

//-----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif
#endif
