
struct OpenPBRBase {
    float weight;
    color col;
    float diffuse_roughness;
    float metalness;
};

struct OpenPBRSpecular {
    float weight;
    color col;
    float roughness;
    float roughness_anisotropy;
    float haze;
    float haze_spread;
    float ior;
};

struct OpenPBRTransmission {
    float weight;
    color col;
    float depth;
    color scatter;
    float scatter_anisotropy;
    float dispersion_scale;
    float dispersion_abbe_number;
};

struct OpenPBRSubsurface {
    float weight;
    color col;
    float radius;
    color radius_scale;
    float scatter_anisotropy;
};

struct OpenPBRCoat {
    float weight;
    color col;
    float roughness;
    float roughness_anisotropy;
    float ior;
    float darkening;
};

struct OpenPBRFuzz {
    float weight;
    color col;
    float roughness;
};

struct OpenPBRThinFilm {
    float weight;
    float thickness;
    float ior;
};

struct OpenPBREmission {
    float luminance;
    color col;
};

struct OpenPBRGeometry {
    float opacity;
    int thin_walled;
    normal norm;
    normal coat_normal;
    vector tangent;
    vector coat_tangent;
};


// One struct to rule them all
struct OpenPBRSurface {
    OpenPBRBase base;
    OpenPBRSpecular specular;
    OpenPBRTransmission transmission;
    OpenPBRSubsurface subsurface;
    OpenPBRCoat coat;
    OpenPBRFuzz fuzz;
    OpenPBRThinFilm thin_film;
    OpenPBREmission emission;
    OpenPBRGeometry geometry;
};