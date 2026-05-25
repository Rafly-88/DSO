#include "util/AnypointParams.h"
#include <fstream>
#include <cstdio>

namespace dso {

static std::mutex     g_mu;
static AnypointParams g_p;

AnypointParams getAnypointParams() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_p;
}

void setAnypointParams(const AnypointParams& p) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_p = p;
}

bool loadAnypointPolynomial(const std::string& path) {
    std::ifstream f(path);
    if (!f.good()) {
        printf("[anypoint] no calibration file at %s, using slider value for p5\n",
               path.c_str());
        return false;
    }
    float p0=0, p1=0, p2=0, p3=0, p4=0, p5=0;
    f >> p0 >> p1 >> p2 >> p3 >> p4 >> p5;
    if (f.fail()) {
        printf("[anypoint] failed to parse 6 floats from %s\n", path.c_str());
        return false;
    }
    AnypointParams ap = getAnypointParams();
    ap.p[0]=p0; ap.p[1]=p1; ap.p[2]=p2; ap.p[3]=p3; ap.p[4]=p4; ap.p[5]=p5;
    ap.poly5 = p5;
    setAnypointParams(ap);
    printf("[anypoint] polynomial loaded from %s: %.4g %.4g %.4g %.4g %.4g %.4g\n",
           path.c_str(), p0, p1, p2, p3, p4, p5);
    return true;
}

} // namespace dso
