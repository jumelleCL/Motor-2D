#pragma once
#include <cmath>

struct Vector
{
    float x, y;
    float magnitud() const;
    float magnitud_quadrada() const;
    float prod_escalar(const Vector& v) const;
    Vector perpendicular() const;
    void normalitzar();
    Vector normalitzat() const;
};

struct Punt
{
    float x, y;
    Punt operator+(const Vector& v) const;
    Punt operator-(const Vector& v) const;
    Vector desplacament(const Punt& p) const;
};