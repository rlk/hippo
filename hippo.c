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

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include "hippo.h"

//-----------------------------------------------------------------------------

#define LY_PER_PC 3.26163344
#define MAXRECLEN 512

// The node structure represents one node in the binary space partitioning of
// the star catalog.

struct node
{
    float    bound[6];
    uint32_t star0;
    uint32_t starc;
    uint32_t nodeL;
    uint32_t nodeR;
};

typedef struct node node;

// The hippo structure represents an open catalog with its stars, BSP nodes,
// and the pointer and length of its mapped file, if any.

struct hippo
{
    star    *stars;
    uint32_t starc;
    node    *nodes;
    uint32_t nodec;

    int      fd;
    void    *ptr;
    size_t   len;
};

//-----------------------------------------------------------------------------

static inline float min(float a, float b)
{
    return (a < b) ? a : b;
}

static inline float max(float a, float b)
{
    return (a > b) ? a : b;
}

// Star-sorting callbacks. Compare the X, Y, or Z coordinates of two stars.

static int star_cmp0(const void *a, const void *b)
{
    return (((const star *) a)->pos[0] <
            ((const star *) b)->pos[0]) ? -1 : +1;
}

static int star_cmp1(const void *a, const void *b)
{
    return (((const star *) a)->pos[1] <
            ((const star *) b)->pos[1]) ? -1 : +1;
}

static int star_cmp2(const void *a, const void *b)
{
    return (((const star *) a)->pos[2] <
            ((const star *) b)->pos[2]) ? -1 : +1;
}

int (*star_cmp[3])(const void *, const void *) = {
    star_cmp0,
    star_cmp1,
    star_cmp2
};

// Recursively sort the list of stars into a binary-space-partitioning.

static int mknode(node *N, uint32_t n0, uint32_t n1, uint32_t d,
                  star *S, uint32_t s0, uint32_t s1, uint32_t i)
{
    // This node contains stars s0 through s1.

    N[n0].starc = s1 - s0;
    N[n0].star0 = s0;

    // If we have space for two more nodes, subdivide.

    if (d > 0)
    {
        uint32_t sm = (s1 + s0) / 2;

        // Sort these stars along the i-axis.

        qsort(S + s0, s1 - s0, sizeof (struct star), star_cmp[i]);

        // Create a BSP split at the i-position of the middle star.

        N[n0].nodeL = n1++;
        N[n0].nodeR = n1++;

        // Create new nodes, each containing half of the stars.

        n1 = mknode(N, N[n0].nodeL, n1, d - 1, S, s0, sm, (i + 1) % 3);
        n1 = mknode(N, N[n0].nodeR, n1, d - 1, S, sm, s1, (i + 1) % 3);

        // Find the node bound.

        N[n0].bound[0] = min(N[N[n0].nodeL].bound[0], N[N[n0].nodeR].bound[0]);
        N[n0].bound[1] = min(N[N[n0].nodeL].bound[1], N[N[n0].nodeR].bound[1]);
        N[n0].bound[2] = min(N[N[n0].nodeL].bound[2], N[N[n0].nodeR].bound[2]);
        N[n0].bound[3] = max(N[N[n0].nodeL].bound[3], N[N[n0].nodeR].bound[3]);
        N[n0].bound[4] = max(N[N[n0].nodeL].bound[4], N[N[n0].nodeR].bound[4]);
        N[n0].bound[5] = max(N[N[n0].nodeL].bound[5], N[N[n0].nodeR].bound[5]);
    }
    else
    {
        // Find the node bound.

        N[n0].bound[0] = N[n0].bound[3] = S[s0].pos[0];
        N[n0].bound[1] = N[n0].bound[4] = S[s0].pos[1];
        N[n0].bound[2] = N[n0].bound[5] = S[s0].pos[2];

        for (uint32_t s = s0; s < s1; s++)
        {
            N[n0].bound[0] = min(N[n0].bound[0], S[s].pos[0]);
            N[n0].bound[1] = min(N[n0].bound[1], S[s].pos[1]);
            N[n0].bound[2] = min(N[n0].bound[2], S[s].pos[2]);
            N[n0].bound[3] = max(N[n0].bound[3], S[s].pos[0]);
            N[n0].bound[4] = max(N[n0].bound[4], S[s].pos[1]);
            N[n0].bound[5] = max(N[n0].bound[5], S[s].pos[2]);
        }
    }
    return n1;
}

//-----------------------------------------------------------------------------

static inline double rad(double d)
{
    return d * 0.017453292519943295;
}

// Parse the given line as a Hipparcos record and populate the star structure.

static int parse_hip(star *s, const char *rec)
{
    double r;  // Right ascension
    double d;  // Declination
    double p;  // Parallax
    double b;  // B magnitude
    double v;  // V magnitude

    if (sscanf(rec +  51, "%lf", &r) == 1 &&
        sscanf(rec +  64, "%lf", &d) == 1 &&
        sscanf(rec +  79, "%lf", &p) == 1 &&
        sscanf(rec + 217, "%lf", &b) == 1 &&
        sscanf(rec + 230, "%lf", &v) == 1 && p > 0.0)
    {
        s->pos[0] = (float) (sin(rad(r)) * cos(rad(d)) * 3261.63344 / fabs(p));
        s->pos[1] = (float) (              sin(rad(d)) * 3261.63344 / fabs(p));
        s->pos[2] = (float) (cos(rad(r)) * cos(rad(d)) * 3261.63344 / fabs(p));
        s->mag[0] = (float) b;
        s->mag[1] = (float) v;

        return 1;
    }
    return 0;
}

// Parse the given line as a Tycho-2 record and populate the star structure.
// Include only records with both B and V magnitudes, and exclude any record
// that already appears in the Hipparcos catalog.

static int parse_tyc(star *s, const char *rec)
{
    int    h;  // Hipparcos number
    double r;  // Right ascension
    double d;  // Declination
    double b;  // B magnitude
    double v;  // V magnitude

    if (sscanf(rec + 142, "%d",  &h) == 0 &&
        sscanf(rec +  15, "%lf", &r) == 1 &&
        sscanf(rec +  28, "%lf", &d) == 1 &&
        sscanf(rec + 110, "%lf", &b) == 1 &&
        sscanf(rec + 123, "%lf", &v) == 1)
    {
        s->pos[0] = (float) (sin(rad(r)) * cos(rad(d)) * 32.6163344);
        s->pos[1] = (float) (              sin(rad(d)) * 32.6163344);
        s->pos[2] = (float) (cos(rad(r)) * cos(rad(d)) * 32.6163344);
        s->mag[0] = (float) b;
        s->mag[1] = (float) v;

        return 1;
    }
    return 0;
}

// Initialize the star array using the given record parser on the named file.

static int parse_dat(hippo *H, const char *filename,
         int (*parse)(star *s, const char *rec))
{
    FILE *stream;

    if ((stream = fopen(filename, "r")))
    {
        // Make a pass over the input to determine the number of records.

        char buf[MAXRECLEN];
        int  n = 0;
        star s;

        while (fgets(buf, MAXRECLEN, stream))
            n += parse(&s, buf);

        // Allocate storage and read all records to it.

        rewind(stream);

        if ((H->stars = (star *) malloc(n * sizeof (star))))
        {
            while (fgets(buf, MAXRECLEN, stream))
                H->starc += parse(H->stars + H->starc, buf);
        }

        fclose(stream);
    }
    return H->starc;
}

//-----------------------------------------------------------------------------

// Read a catalog in Hipparcos format and generate its index.

hippo *hippo_read_hip(const char *filename, uint32_t d)
{
    hippo *H;

    if ((H = (hippo *) calloc(sizeof (hippo), 1)))
    {
        if (parse_dat(H, filename, parse_hip) > 0)
        {
            if ((H->nodes = (node *) malloc((1 << (d + 1)) * sizeof(node))))
            {
                H->nodec = mknode(H->nodes, 0, 1, d, H->stars, 0, H->starc, 0);
                return H;
            }
        }
    }
    hippo_free(H);
    return NULL;
}

// Read a catalog in Tycho-2 format and generate its index.

hippo *hippo_read_tyc(const char *filename, uint32_t d)
{
    hippo *H;

    if ((H = (hippo *) calloc(sizeof (hippo), 1)))
    {
        if (parse_dat(H, filename, parse_tyc) > 0)
        {
            if ((H->nodes = (node *) malloc((1 << (d + 1)) * sizeof(node))))
            {
                H->nodec = mknode(H->nodes, 0, 1, d, H->stars, 0, H->starc, 0);
                return H;
            }
        }
    }
    hippo_free(H);
    return H;
}

//-----------------------------------------------------------------------------

// Return a four-character code of the given string.

static uint32_t fourcc(const char *s)
{
    return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
}

// Search a RIFF and return a pointer to the chunk with the given FOURCC.

static void *riff_chunk(void *p, uint32_t cc)
{
    uint32_t *b = (uint32_t *) p;

    if (b[0] == fourcc("RIFF"))

        for (uint32_t *c = b + 2; c < b + 2 + b[1] / 4;
                                  c = c + 2 + c[1] / 4)
            if (c[0] == cc)
                return c;

    return 0;
}

// Read a catalog from the named file in RIFF format.

hippo *hippo_read(const char *filename)
{
    struct stat st;
    hippo       *H;
    uint32_t    *c;

    if ((H = (hippo *) calloc(sizeof (hippo), 1)))
    {
        if ((H->fd = open(filename, O_RDONLY)) != -1)
        {
            if ((fstat(H->fd, &st)) != -1)
            {
                H->ptr =  mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, H->fd, 0);
                H->len = (size_t) st.st_size;

                if ((c = (uint32_t *) riff_chunk(H->ptr, fourcc("STAR"))))
                {
                    H->stars =   (star *) (c + 2);
                    H->starc = (uint32_t) (c[1] / sizeof (star));
                }

                if ((c = (uint32_t *) riff_chunk(H->ptr, fourcc("NODE"))))
                {
                    H->nodes =   (node *) (c + 2);
                    H->nodec = (uint32_t) (c[1] / sizeof (node));
                }
                return H;
            }
        }
    }
    hippo_free(H);
    return NULL;
}

// Release all resources held by this catalog.

void hippo_free(hippo *H)
{
    if (H)
    {
        if (H->fd)
        {
            if (H->ptr) munmap(H->ptr, H->len);
            close(H->fd);
        }
        else
        {
            if (H->nodes) free(H->nodes);
            if (H->stars) free(H->stars);
        }
        free(H);
    }
}

// Write the catalog contents to the named file in RIFF format.

int hippo_write(hippo *H, const char *filename)
{
    int fd   = 0;
    int stat = 0;

    assert(sizeof (float) == 4);

    if (H && (fd = open(filename, O_WRONLY | O_CREAT, 0666)) != -1)
    {
        uint32_t stars = (uint32_t) (H->starc * sizeof (star));
        uint32_t nodes = (uint32_t) (H->nodec * sizeof (node));
        uint32_t riffs = stars + nodes + 16;

        stat = (write(fd, "RIFF",   4) == 4
             && write(fd, &riffs,   4) == 4
             && write(fd, "STAR",   4) == 4
             && write(fd, &stars,   4) == 4
             && write(fd, H->stars, (size_t) stars) == (ssize_t) stars
             && write(fd, "NODE",   4) == 4
             && write(fd, &nodes,   4) == 4
             && write(fd, H->nodes, (size_t) nodes) == (ssize_t) nodes);

        close(fd);
    }
    return stat;
}

//-----------------------------------------------------------------------------

// Return the number of bounding box corners lying in front of plane v.

static int plane_test(const float *b, const float *v)
{
    const float k00 = b[0] * v[0];
    const float k11 = b[1] * v[1];
    const float k22 = b[2] * v[2];
    const float k30 = b[3] * v[0];
    const float k41 = b[4] * v[1];
    const float k52 = b[5] * v[2];

    return (k00 + k11 + k22 + v[3] > 0 ? 1 : 0)
         + (k00 + k11 + k52 + v[3] > 0 ? 1 : 0)
         + (k00 + k41 + k22 + v[3] > 0 ? 1 : 0)
         + (k00 + k41 + k52 + v[3] > 0 ? 1 : 0)
         + (k30 + k11 + k22 + v[3] > 0 ? 1 : 0)
         + (k30 + k11 + k52 + v[3] > 0 ? 1 : 0)
         + (k30 + k41 + k22 + v[3] > 0 ? 1 : 0)
         + (k30 + k41 + k52 + v[3] > 0 ? 1 : 0);
}

// Determine whether an axis-aligned bounding box b is outside-of, inside-of,
// or split-by the set of c planes at v.

static int bound_test(const float *b, const float *v, int c)
{
    int n, m = 0;

    // If the box is entirely behind one of the planes, return -1.

    for (int i = 0; i < c; i++)
        if ((n = plane_test(b, v + i * 4)) == 0)
            return -1;
        else
            m += n;

    // If the box is entirely in front of all planes, return +1.

    return (m == c * 8) ? 1 : 0;
}

// Traverse the node hierarchy. Call fn with each list of stars that falls
// within the set of c planes at v.

static void traverse(const hippo *H,
                     const float *v, int c, hippo_seek_fn fn, uint32_t n, int i)
{
    int r = bound_test(H->nodes[n].bound, v, c);

    if (r >= 0)
    {
        if (r > 0 || H->nodes[n].nodeL == 0 || H->nodes[n].nodeR == 0)
            fn(H->stars + H->nodes[n].star0, H->nodes[n].starc);
        else
        {
            traverse(H, v, c, fn, H->nodes[n].nodeL, (i + 1) % 3);
            traverse(H, v, c, fn, H->nodes[n].nodeR, (i + 1) % 3);
        }
    }
}

// Call fn with each list of stars that falls within the set of c planes at v.

void hippo_seek(const hippo *H, const float *v, int c, hippo_seek_fn fn)
{
    traverse(H, v, c, fn, 0, 0);
}

// Return a pointer to the array of stars.

const star *hippo_data(const hippo *H)
{
    return H->stars;
}

// Return the number of stars in the catalog.

uint32_t hippo_size(const hippo *H)
{
    return H->starc;
}

//-----------------------------------------------------------------------------

// Compute and return the six bounding planes of the model-view-projection
// matrix M.

void hippo_view_bound(float *v, const float *M)
{
    int i;

    // Left plane

    v[ 0] = M[12] + M[ 0];
    v[ 1] = M[13] + M[ 1];
    v[ 2] = M[14] + M[ 2];
    v[ 3] = M[15] + M[ 3];

    // Right plane

    v[ 4] = M[12] - M[ 0];
    v[ 5] = M[13] - M[ 1];
    v[ 6] = M[14] - M[ 2];
    v[ 7] = M[15] - M[ 3];

    // Bottom plane

    v[ 8] = M[12] + M[ 4];
    v[ 9] = M[13] + M[ 5];
    v[10] = M[14] + M[ 6];
    v[11] = M[15] + M[ 7];

    // Top plane

    v[12] = M[12] - M[ 4];
    v[13] = M[13] - M[ 5];
    v[14] = M[14] - M[ 6];
    v[15] = M[15] - M[ 7];

    // Near plane

    v[16] = M[12] + M[ 8];
    v[17] = M[13] + M[ 9];
    v[18] = M[14] + M[10];
    v[19] = M[15] + M[11];

    // Far plane

    v[20] = M[12] - M[ 8];
    v[21] = M[13] - M[ 9];
    v[22] = M[14] - M[10];
    v[23] = M[15] - M[11];

    // Normalize all plane vectors

    for (i = 0; i < 6; ++i)
    {
        float k = (float) sqrt(v[4 * i + 0] * v[4 * i + 0]
                             + v[4 * i + 1] * v[4 * i + 1]
                             + v[4 * i + 2] * v[4 * i + 2]);
        v[i * 4 + 0] /= k;
        v[i * 4 + 1] /= k;
        v[i * 4 + 2] /= k;
        v[i * 4 + 3] /= k;
    }
}

// Compute and return the six bounding planes of the axis-aligned bounding
// box centered at p, enclosing the volume to distance d.

void hippo_cube_bound(float *v, const float *p, float d)
{
    // -X plane

    v[ 0] =  1.0f;
    v[ 1] =  0.0f;
    v[ 2] =  0.0f;
    v[ 3] = -p[0] + d;

    // +X plane

    v[ 4] = -1.0f;
    v[ 5] =  0.0f;
    v[ 6] =  0.0f;
    v[ 7] =  p[0] + d;

    // -Y plane

    v[ 8] =  0.0f;
    v[ 9] =  1.0f;
    v[10] =  0.0f;
    v[11] = -p[1] + d;

    // +Y plane

    v[12] =  0.0f;
    v[13] = -1.0f;
    v[14] =  0.0f;
    v[15] =  p[1] + d;

    // -Z plane

    v[16] =  0.0f;
    v[17] =  0.0f;
    v[18] =  1.0f;
    v[19] = -p[2] + d;

    // +Z plane

    v[20] =  0.0f;
    v[21] =  0.0f;
    v[22] = -1.0f;
    v[23] =  p[2] + d;
}

//-----------------------------------------------------------------------------
