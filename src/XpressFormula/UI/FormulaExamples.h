// FormulaExamples.h - Built-in example formulas for the editor and presets.
#pragma once

#include <array>

namespace XpressFormula::UI {

struct ExamplePattern {
    const char* label;
    const char* expression;
    const bool includeInPresets = true;
};

// Example patterns to show in the editor reference. The same list also feeds
// the presets, with includeInPresets available for reference-only entries.
inline constexpr auto kExamplePatterns = std::to_array<ExamplePattern>({
    // {label, expression, includeInPresets}
    { "Damped sine wave", "sin(x) * exp(-x*x/12)" },
    { "Cosine curve", "y = cos(x)" },
    { "Sine-cosine surface", "z = sin(x) * cos(y)" },
    { "Square-root saddle", "sqrt(abs(x*y))" },
    { "Radial sine waves", "sin(sqrt(x^2+y^2))" },
    { "Interference waves", "sin(sqrt(x^2+y^2))+sin(1.1*sqrt((x-10)^2+y^2))+sin(1.4*sqrt((x-10)^2+(y-10)^2))+sin(2.1*sqrt((x)^2+(y-10)^2))" },
    { "Circle", "x^2 + y^2 = 100" },
    { "Ellipse", "pow(x,2)/25 + pow(y,2)/9 = 1" },
    { "Implicit interference", "sin(sqrt(x^2+y^2))+sin(1.1*sqrt((x-10)^2+y^2))+sin(1.4*sqrt((x-10)^2+(y-10)^2))+sin(2.1*sqrt((x)^2+(y-10)^2))=0" },
    { "Sphere", "x^2 + y^2 + z^2 = 16" },
    { "Large torus", "(x^2+y^2+z^2+21)^2 - 100*(x^2+y^2) = 0" },
    { "Torus", "(x^2+y^2+z^2+5)^2 - 36*(x^2+y^2) = 0" },
    { "Superelliptic torus", "pow(abs(pow(pow(abs(x),4)+pow(abs(y),4),0.25)-1.0),4)+pow(abs(z),4)=pow(0.35,4)" },
    { "Rounded shell", "max(pow(pow(abs(x/1.25),6)+pow(abs(y/1.00),6)+pow(abs(z/0.82),6),1.0/6)-1,1-sqrt(pow(abs(y)/(0.28+0.17*pow(abs(x/1.25),4)),2)+pow(abs(z)/(0.24+0.15*pow(abs(x/1.25),4)),2)))=0" },
    { "Rounded cube with variable tunnels", "max(pow(pow(abs(x/1.18),8)+pow(abs(y/1.02),8)+pow(abs(z/0.88),8),1.0/8)-1,-min(sqrt(y^2+z^2)-(0.22+0.20*pow(abs(x/1.18),4)),min(sqrt(x^2+z^2)-(0.22+0.20*pow(abs(y/1.02),4)),sqrt(x^2+y^2)-(0.22+0.20*pow(abs(z/0.88),4)))))=0" },
    { "Linear scalar field", "sin(x) + cos(y) + z = 0" },

    // Decorative and mathematically interesting implicit 3D surfaces.
    { "Gyroid", "sin(x)*cos(y)+sin(y)*cos(z)+sin(z)*cos(x)=0" },
    { "Twisted gyroid", "sin(x+0.35*z)*cos(y)+sin(y+0.35*x)*cos(z)+sin(z+0.35*y)*cos(x)=0" },
    { "Schwarz P surface", "cos(x)+cos(y)+cos(z)=0" },
    { "Rounded cube with tunnels", "max(pow(pow(abs(x/1.2),8)+pow(abs(y/1.2),8)+pow(abs(z/1.2),8),1.0/8)-1,-min(min(sqrt(y^2+z^2)-0.28,sqrt(x^2+z^2)-0.28),sqrt(x^2+y^2)-0.28))=0" },
    { "Three-lobed torus", "pow(sqrt(x^2+y^2)-1.4-0.25*cos(3*atan2(y,x)),2)+z^2-0.12=0" },
    { "Heart", "pow(x^2+(9.0/4.0)*y^2+z^2-1,3)-x^2*z^3-(9.0/80.0)*y^2*z^3=0" },
    { "Metaball molecule", "1/(x^2+y^2+z^2+0.08)+1/((x-1)^2+y^2+z^2+0.08)+1/((x+1)^2+y^2+z^2+0.08)+1/(x^2+(y-1)^2+z^2+0.08)+1/(x^2+(y+1)^2+z^2+0.08)-8=0" },
    { "Wavy superellipsoid", "pow(pow(abs(x/1.4),6)+pow(abs(y/1.1),6)+pow(abs(z/0.9),6),1.0/6)-1+0.08*sin(8*x)*sin(8*y)*sin(8*z)=0" },
    { "Spiral seed pod", "pow(pow(abs(x),4)+pow(abs(y),4)+pow(abs(z/1.6),4),1.0/4)-1+0.12*sin(10*atan2(y,x)+4*z)=0" },
    { "Symmetric cage", "max(pow(pow(abs(x/1.4),10)+pow(abs(y/1.4),10)+pow(abs(z/1.4),10),1.0/10)-1,-min(min(abs(x*y)-0.12,abs(y*z)-0.12),abs(z*x)-0.12))=0" },
});

} // namespace XpressFormula::UI
