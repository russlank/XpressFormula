// ExampleFormulaCatalog.cpp - Built-in formula examples for UI and tests.
#include "ExampleFormulaCatalog.h"

#include <array>

namespace XpressFormula::Core {

namespace {

constexpr auto kExamples = std::to_array<ExampleFormula>({
    { "Damped sine wave", "sin(x) * exp(-x*x/12)", "A decaying 2D wave that combines sine with exponential falloff." },
    { "Cosine curve", "y = cos(x)", "A simple explicit 2D cosine curve." },
    { "Sine-cosine surface", "z = sin(x) * cos(y)", "A smooth explicit surface made from crossing sine and cosine waves." },
    { "Square-root saddle", "sqrt(abs(x*y))", "A folded surface that uses absolute value to keep the square root in range." },
    { "Radial sine waves", "sin(length2(x,y))", "Concentric waves using length2 for radial distance from the origin." },
    { "Interference waves", "sin(length2(x,y))+sin(1.1*distance2(x,y,10,0))+sin(1.4*distance2(x,y,10,10))+sin(2.1*distance2(x,y,0,10))", "Several radial wave sources interfere across the plane." },
    { "Circle", "length2(x,y) = 10", "A 2D implicit circle written with the length2 distance helper." },
    { "Ellipse", "pow(x,2)/25 + pow(y,2)/9 = 1", "A classic implicit ellipse with different x and y radii." },
    { "Implicit interference", "sin(length2(x,y))+sin(1.1*distance2(x,y,10,0))+sin(1.4*distance2(x,y,10,10))+sin(2.1*distance2(x,y,0,10))=0", "A contour version of the interference-wave field." },
    { "Sphere", "length3(x,y,z) = 4", "A centered implicit sphere using the length3 distance helper." },
    { "Large torus", "sdTorus(x,y,z,5,2)=0", "A large signed-distance torus around the Z axis." },
    { "Torus", "sdTorus(x,y,z,3,2)=0", "A compact signed-distance torus around the Z axis." },
    { "Superelliptic torus", "pow(abs(pow(pow(abs(x),4)+pow(abs(y),4),0.25)-1.0),4)+pow(abs(z),4)=pow(0.35,4)", "A torus-like implicit shape with squarer superellipse cross-sections." },
    { "Rounded shell", "max(pow(pow(abs(x/1.25),6)+pow(abs(y/1.00),6)+pow(abs(z/0.82),6),1.0/6)-1,1-length2(abs(y)/(0.28+0.17*pow(abs(x/1.25),4)),abs(z)/(0.24+0.15*pow(abs(x/1.25),4))))=0", "A rounded implicit shell with a variable oval opening." },
    { "Rounded cube with variable tunnels", "max(pow(pow(abs(x/1.18),8)+pow(abs(y/1.02),8)+pow(abs(z/0.88),8),1.0/8)-1,-min(length2(y,z)-(0.22+0.20*pow(abs(x/1.18),4)),min(length2(x,z)-(0.22+0.20*pow(abs(y/1.02),4)),length2(x,y)-(0.22+0.20*pow(abs(z/0.88),4)))))=0", "A rounded cube-like surface pierced by three variable-width tunnels." },
    { "Linear scalar field", "sin(x) + cos(y) + z = 0", "A simple implicit 3D scalar field with one linear axis." },

    { "Gyroid", "sin(x)*cos(y)+sin(y)*cos(z)+sin(z)*cos(x)=0", "A classic triply periodic implicit surface." },
    { "Twisted gyroid", "sin(x+0.35*z)*cos(y)+sin(y+0.35*x)*cos(z)+sin(z+0.35*y)*cos(x)=0", "A gyroid variant with coordinate coupling for a twisted feel." },
    { "Schwarz P surface", "cos(x)+cos(y)+cos(z)=0", "A classic triply periodic minimal-surface style formula." },
    { "Rounded cube with tunnels", "max(pow(pow(abs(x/1.2),8)+pow(abs(y/1.2),8)+pow(abs(z/1.2),8),1.0/8)-1,-min(min(length2(y,z)-0.28,length2(x,z)-0.28),length2(x,y)-0.28))=0", "A rounded cube-like implicit shape with three fixed circular tunnels." },
    { "Three-lobed torus", "pow(length2(x,y)-1.4-0.25*cos(3*atan2(y,x)),2)+z^2-0.12=0", "A torus whose radius is modulated by angle into three lobes." },
    { "Heart", "pow(x^2+(9.0/4.0)*y^2+z^2-1,3)-x^2*z^3-(9.0/80.0)*y^2*z^3=0", "A well-known implicit heart surface." },
    { "Metaball molecule", "1/(x^2+y^2+z^2+0.08)+1/((x-1)^2+y^2+z^2+0.08)+1/((x+1)^2+y^2+z^2+0.08)+1/(x^2+(y-1)^2+z^2+0.08)+1/(x^2+(y+1)^2+z^2+0.08)-8=0", "Several inverse-square blobs merge into a molecule-like implicit surface." },
    { "Wavy superellipsoid", "pow(pow(abs(x/1.4),6)+pow(abs(y/1.1),6)+pow(abs(z/0.9),6),1.0/6)-1+0.08*sin(8*x)*sin(8*y)*sin(8*z)=0", "A rounded superellipsoid with fine periodic surface ripples." },
    { "Spiral seed pod", "pow(pow(abs(x),4)+pow(abs(y),4)+pow(abs(z/1.6),4),1.0/4)-1+0.12*sin(10*atan2(y,x)+4*z)=0", "A pod-like implicit shape with angular spiral ridges." },
    { "Symmetric cage", "max(pow(pow(abs(x/1.4),10)+pow(abs(y/1.4),10)+pow(abs(z/1.4),10),1.0/10)-1,-min(min(abs(x*y)-0.12,abs(y*z)-0.12),abs(z*x)-0.12))=0", "A symmetric cage formed by combining a rounded boundary with plane-pair openings." },

    { "Wavy radial surface", "z = sin(8*length2(x,y))*smoothstep(3,0,length2(x,y))", "A radial sine surface faded with smoothstep for a soft edge." },
    { "Asteroid sphere", "length3(x,y,z)-1+0.15*noise3(5*x,5*y,5*z)=0", "A noisy implicit sphere that demonstrates length3 and noise3 to create an asteroid-like surface." },
    { "Smooth double blob", "smin(sdSphere(x-0.7,y,z,0.5),sdSphere(x+0.7,y,z,0.5),0.25)=0", "Two spheres blended with smin to create a smooth union." },
    { "Box with round tunnel", "smax(sdBox(x,y,z,1,1,1),-(sdCylinderX(x,y,z,0.32)),0.12)=0", "A centered box with a softly subtracted round tunnel through the X axis." },
    { "Wavy torus", "sdTorus(x,y,z,1.2,0.25+0.06*sin(8*atan2(y,x)))=0", "A signed-distance torus with angular tube-radius modulation." },
    { "Noisy terrain", "z = 0.35*fbm2(2*x,2*y)", "A simple height-field terrain using normalized 2D fractal noise." },
    { "Repeated cell pattern", "abs(fract(x)-0.5)+abs(fract(y)-0.5)-0.25=0", "A tiled 2D contour pattern built from fractional coordinates." },
    { "Morphed sphere cube", "mix(length3(x,y,z)-1,pow(abs(x),4)+pow(abs(y),4)+pow(abs(z),4)-1,0.5)=0", "A blend between a sphere field and a super-cube-like field using mix." },
});

} // namespace

std::span<const ExampleFormula> exampleFormulas() {
    return kExamples;
}

} // namespace XpressFormula::Core
