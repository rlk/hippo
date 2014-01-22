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

#include <stdint.h>

#include "hippo.h"
#include "gl.hpp"

using namespace gl;

//-----------------------------------------------------------------------------

static float window_aspect = 1;
static int   window_w      = 0;
static int   window_h      = 0;

static GLuint program =  0;
static GLint  ploc    = -1;
static GLint  mloc    = -1;
static GLint  Ploc    = -1;
static GLint  Mloc    = -1;
static GLint  bloc    = -1;

static float fov = 45.0f;
static vec3  view_rotation;
static vec3  view_position;
static vec3  view_movement;

static hippo *H = 0;
static hippo *T = 0;

static GLuint H_vao;
static GLuint T_vao;
static GLuint tex;

static vec3  click_rotation;
static float click_fov;
static int   click_x;
static int   click_y;
static int   click_button;

static const GLfloat spectrum[6][3] = {
    { 0.0, 0.0, 1.0 },
    { 1.0, 1.0, 1.0 },
    { 1.0, 1.0, 0.5 },
    { 1.0, 1.0, 0.0 },
    { 1.0, 0.5, 0.0 },
    { 1.0, 0.0, 0.0 },
};

//-----------------------------------------------------------------------------

bool animating()
{
    return (view_movement[0] || view_movement[1] || view_movement[2]);
}

mat4 orientation(vec3 r)
{
    return xrotation(to_radians(r[0]))
         * yrotation(to_radians(r[1]));
}

mat4 occidentation(vec3 r)
{
    return yrotation(to_radians(-r[1]))
         * xrotation(to_radians(-r[0]));
}

GLuint init_vao(hippo *H)
{
    GLuint vao = 0;
    GLuint vbo = 0;

    if (H)
    {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,  hippo_size(H) * sizeof (star),
                                       hippo_data(H), GL_STATIC_DRAW);

        glEnableVertexAttribArray(ploc);
        glEnableVertexAttribArray(mloc);

        glVertexAttribPointer(ploc, 3, GL_FLOAT, GL_FALSE, sizeof (star), (const void *)  0);
        glVertexAttribPointer(mloc, 2, GL_FLOAT, GL_FALSE, sizeof (star), (const void *) 12);

        glBindVertexArray(0);
    }
    return vao;
}

void init()
{
    if ((program = init_program("star-150.vert", "star-150.frag")))
    {
        ploc = glGetAttribLocation (program, "Position");
        mloc = glGetAttribLocation (program, "Magnitude");
        Ploc = glGetUniformLocation(program, "P");
        Mloc = glGetUniformLocation(program, "M");
        bloc = glGetUniformLocation(program, "brightness");
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 6, 1, 0, GL_RGB, GL_FLOAT, spectrum);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if ((H = hippo_read("hipparcos.riff"))) H_vao = init_vao(H);
    if ((T = hippo_read("tycho.riff")))     T_vao = init_vao(T);

#ifdef GL_POINT_SPRITE
    glEnable(GL_POINT_SPRITE);
#endif
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
}

void draw_H(const star *v, uint32_t c)
{
    glDrawArrays(GL_POINTS, v - hippo_data(H), c);
}

void draw_T(const star *v, uint32_t c)
{
    glDrawArrays(GL_POINTS, v - hippo_data(T), c);
}

void draw()
{
    float v[24];

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 P = perspective(to_radians(fov), window_aspect, 1.f, 10000.f);

    glUseProgram(program);
    glUniformMatrix4fv(Ploc, 1, GL_TRUE, P);
    glUniform1f       (bloc, 32.0f * 45.0f / fov);

    if (H)
    {
        mat4 M = orientation(view_rotation)
               * translation(view_position);

        hippo_view_bound(v, P * M);

        glUniformMatrix4fv(Mloc, 1, GL_TRUE, M);
        glBindVertexArray(H_vao);
        hippo_seek(H, v, 6, draw_H);
    }
    if (T)
    {
        mat4 M = orientation(view_rotation);

        hippo_view_bound(v, P * M);

        glUniformMatrix4fv(Mloc, 1, GL_TRUE, M);
        glBindVertexArray(T_vao);
        hippo_seek(T, v, 6, draw_T);
    }
}

void step()
{
    if (length(view_movement) > 0.0)
        view_position = view_position + normal(occidentation(view_rotation)) * normalize(view_movement);
}

void resize(int width, int height)
{
    glViewport(0, 0, width, height);

    window_aspect = float(width) / float(height);
    window_w      = width;
    window_h      = height;
}

void click(int button, bool state, int x, int y)
{
    click_button   = button;
    click_rotation = view_rotation;
    click_fov      = fov;
    click_x        = x;
    click_y        = y;
}

void point(int x, int y)
{
    GLfloat dx = GLfloat(x - click_x) / window_w;
    GLfloat dy = GLfloat(y - click_y) / window_h;

    if (click_button == 0)
    {
        view_rotation[0] = click_rotation[0] + 2.0 * fov * dy;
        view_rotation[1] = click_rotation[1] + 2.0 * fov * dx;
    }
    if (click_button == 1)
    {
        view_rotation[2] = click_rotation[2] + 2.0 * fov * dx;
    }
    if (click_button == 2)
    {
        fov = click_fov - 45.0 * dy;
    }

    if (view_rotation[0] >  90.0f) view_rotation[0] =  90.0f;
    if (view_rotation[0] < -90.0f) view_rotation[0] = -90.0f;
}

void key(int k, bool state)
{
    float d = state ? +1 : -1;

    switch (k)
    {
        case 'a': view_movement[0] += d; break;
        case 'e': view_movement[0] -= d; break;
        case 'j': view_movement[1] += d; break;
        case ' ': view_movement[1] -= d; break;
        case ',': view_movement[2] += d; break;
        case 'o': view_movement[2] -= d; break;

        case 27: exit(0);
    }
}

//-----------------------------------------------------------------------------
