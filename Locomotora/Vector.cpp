#include "Vector.h"

Punt Punt::operator+(const Vector& v) const
{
    return Punt{ x + v.x, y + v.y };
}

Punt Punt::operator-(const Vector& v) const
{
    return Punt{ x - v.x, y - v.y };
}

Vector Punt::desplacament(const Punt& p) const
{
    return Vector{ p.x - x, p.y - y };
}

float Vector::magnitud() const
{
    return std::sqrt(x * x + y * y);
}

float Vector::magnitud_quadrada() const
{
    return x * x + y * y;
}

float Vector::prod_escalar(const Vector& v) const
{
    return x * v.x + y * v.y;
}

Vector Vector::perpendicular() const
{
    return Vector{ -y, x };
}

void Vector::normalitzar()
{
    float m = magnitud();
    if (m != 0.0f)
    {
        x /= m; y /= m;
    }
}
Vector Vector::normalitzat() const
{
    Vector v = *this;
    v.normalitzar();
    return v;
}
