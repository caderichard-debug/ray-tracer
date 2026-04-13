#ifndef LIGHT_H
#define LIGHT_H

#include "../math/vec3.h"

class Light {
public:
    Point3 position;
    Color intensity; // RGB intensity

    Light() : position(0, 0, 0), intensity(1, 1, 1) {}
    Light(Point3 pos, Color intens) : position(pos), intensity(intens) {}
};

#endif // LIGHT_H
