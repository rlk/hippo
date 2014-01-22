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

#include <stdio.h>

#ifdef __APPLE__
#  include <GLUT/glut.h>
#else
#  include <GL/glew.h>
#  include <GL/glut.h>
#  define GLUT_3_2_CORE_PROFILE 0
#endif

extern bool animating();
extern void init();
extern void draw();
extern void step();
extern void resize(int, int);
extern void point (int, int);
extern void click (int, bool, int, int);
extern void key   (int, bool);

//-----------------------------------------------------------------------------

static void display()
{
    draw();
    glutSwapBuffers();
}

static void idle()
{
    step();
    glutPostRedisplay();
}

static void update()
{
    if (animating())
        glutIdleFunc(idle);
    else
    {
        glutIdleFunc(NULL);
        glutPostRedisplay();
    }
}

//-----------------------------------------------------------------------------

void mouse(int b, int s, int x, int y)
{
    click(b, s, x, y);
    update();
}

void motion(int x, int y)
{
    point(x, y);
    update();
}

void keyboard(unsigned char k, int x, int y)
{
    key(k, true);
    update();
}

void keyboardup(unsigned char k, int x, int y)
{
    key(k, false);
    update();
}

//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    glutInitDisplayMode(GLUT_DEPTH
                      | GLUT_DOUBLE
                      | GLUT_MULTISAMPLE
                      | GLUT_3_2_CORE_PROFILE);

    glutInitWindowSize(960, 540);
    glutInit(&argc, argv);

    glutCreateWindow(argv[0]);

    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutReshapeFunc(resize);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardup);

    glutIgnoreKeyRepeat(1);

#ifndef __APPLE__
    if (glewInit() == GLEW_OK)
#endif
    {
        printf("OpenGL %s GLSL %s\n",
                glGetString(GL_VERSION),
                glGetString(GL_SHADING_LANGUAGE_VERSION));

        init();
        glutMainLoop();
    }
    return 0;
}
