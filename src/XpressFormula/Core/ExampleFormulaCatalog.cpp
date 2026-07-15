// ExampleFormulaCatalog.cpp - Built-in formula examples for UI and tests.
#include "ExampleFormulaCatalog.h"

#include <array>

namespace XpressFormula::Core {

namespace {

constexpr auto kExamples = std::to_array<ExampleFormula>({
    { "Damped sine wave", "sin(x) * exp(-x*x/12)" },
    { "Cosine curve", "y = cos(x)" },
    { "Sine-cosine surface", "z = sin(x) * cos(y)" },
    { "Square-root saddle", "sqrt(abs(x*y))" },
    { "Radial sine waves", "sin(length2(x,y))" },
    { "Interference waves", "sin(length2(x,y))+sin(1.1*distance2(x,y,10,0))+sin(1.4*distance2(x,y,10,10))+sin(2.1*distance2(x,y,0,10))" },
    { "Circle", "length2(x,y) = 10" },
    { "Ellipse", "pow(x,2)/25 + pow(y,2)/9 = 1" },
    { "Implicit interference", "sin(length2(x,y))+sin(1.1*distance2(x,y,10,0))+sin(1.4*distance2(x,y,10,10))+sin(2.1*distance2(x,y,0,10))=0" },
    { "Sphere", "length3(x,y,z) = 4" },
    { "Large torus", "sdTorus(x,y,z,5,2)=0" },
    { "Torus", "sdTorus(x,y,z,3,2)=0" },
    { "Superelliptic torus", "pow(abs(pow(pow(abs(x),4)+pow(abs(y),4),0.25)-1.0),4)+pow(abs(z),4)=pow(0.35,4)" },
    { "Rounded shell", "max(pow(pow(abs(x/1.25),6)+pow(abs(y/1.00),6)+pow(abs(z/0.82),6),1.0/6)-1,1-length2(abs(y)/(0.28+0.17*pow(abs(x/1.25),4)),abs(z)/(0.24+0.15*pow(abs(x/1.25),4))))=0" },
    { "Rounded cube with variable tunnels", "max(pow(pow(abs(x/1.18),8)+pow(abs(y/1.02),8)+pow(abs(z/0.88),8),1.0/8)-1,-min(length2(y,z)-(0.22+0.20*pow(abs(x/1.18),4)),min(length2(x,z)-(0.22+0.20*pow(abs(y/1.02),4)),length2(x,y)-(0.22+0.20*pow(abs(z/0.88),4)))))=0" },
    { "Linear scalar field", "sin(x) + cos(y) + z = 0" },

    { "Gyroid", "sin(x)*cos(y)+sin(y)*cos(z)+sin(z)*cos(x)=0" },
    { "Twisted gyroid", "sin(x+0.35*z)*cos(y)+sin(y+0.35*x)*cos(z)+sin(z+0.35*y)*cos(x)=0" },
    { "Schwarz P surface", "cos(x)+cos(y)+cos(z)=0" },
    { "Rounded cube with tunnels", "max(pow(pow(abs(x/1.2),8)+pow(abs(y/1.2),8)+pow(abs(z/1.2),8),1.0/8)-1,-min(min(length2(y,z)-0.28,length2(x,z)-0.28),length2(x,y)-0.28))=0" },
    { "Three-lobed torus", "pow(length2(x,y)-1.4-0.25*cos(3*atan2(y,x)),2)+z^2-0.12=0" },
    { "Heart", "pow(x^2+(9.0/4.0)*y^2+z^2-1,3)-x^2*z^3-(9.0/80.0)*y^2*z^3=0" },
    { "Metaball molecule", "1/(x^2+y^2+z^2+0.08)+1/((x-1)^2+y^2+z^2+0.08)+1/((x+1)^2+y^2+z^2+0.08)+1/(x^2+(y-1)^2+z^2+0.08)+1/(x^2+(y+1)^2+z^2+0.08)-8=0" },
    { "Wavy superellipsoid", "pow(pow(abs(x/1.4),6)+pow(abs(y/1.1),6)+pow(abs(z/0.9),6),1.0/6)-1+0.08*sin(8*x)*sin(8*y)*sin(8*z)=0" },
    { "Spiral seed pod", "pow(pow(abs(x),4)+pow(abs(y),4)+pow(abs(z/1.6),4),1.0/4)-1+0.12*sin(10*atan2(y,x)+4*z)=0" },
    { "Symmetric cage", "max(pow(pow(abs(x/1.4),10)+pow(abs(y/1.4),10)+pow(abs(z/1.4),10),1.0/10)-1,-min(min(abs(x*y)-0.12,abs(y*z)-0.12),abs(z*x)-0.12))=0" },

    { "Wavy radial surface", "z = sin(8*length2(x,y))*smoothstep(3,0,length2(x,y))" },
    { "Asteroid sphere", "length3(x,y,z)-1+0.15*noise3(5*x,5*y,5*z)=0" },
    { "Smooth double blob", "smin(sdSphere(x-0.7,y,z,0.5),sdSphere(x+0.7,y,z,0.5),0.25)=0" },
    { "Box with round tunnel", "smax(sdBox(x,y,z,1,1,1),-(sdCylinderX(x,y,z,0.32)),0.12)=0" },
    { "Wavy torus", "sdTorus(x,y,z,1.2,0.25+0.06*sin(8*atan2(y,x)))=0" },
    { "Noisy terrain", "z = 0.35*fbm2(2*x,2*y)" },
    { "Repeated cell pattern", "abs(fract(x)-0.5)+abs(fract(y)-0.5)-0.25=0" },
    { "Morphed sphere cube", "mix(length3(x,y,z)-1,pow(abs(x),4)+pow(abs(y),4)+pow(abs(z),4)-1,0.5)=0" },
});

} // namespace

std::span<const ExampleFormula> exampleFormulas() {
    return kExamples;
}

} // namespace XpressFormula::Core
