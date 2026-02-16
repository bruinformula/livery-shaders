

// Open Shading Language Standard Library
// Stripped version using only OSL built-in types
// Based on MaterialX OSL implementation



float mx_remap(float in, float inLow, float inHigh, float outLow, float outHigh, int doClamp)
{
    float x = (in - inLow)/(inHigh-inLow);
    if (doClamp == 1) {
        x = clamp(x, 0, 1);
    }
    return outLow + (outHigh - outLow) * x;
}

color mx_remap(color in, color inLow, color inHigh, color outLow, color outHigh, int doClamp)
{
    color x = (in - inLow) / (inHigh - inLow);
    if (doClamp == 1) {
        x = clamp(x, 0, 1);
    }
    return outLow + (outHigh - outLow) * x;
}

color mx_remap(color in, float inLow, float inHigh, float outLow, float outHigh, int doClamp)
{
    color x = (in - inLow) / (inHigh - inLow);
    if (doClamp == 1) {
        x = clamp(x, 0, 1);
    }
    return outLow + (outHigh - outLow) * x;
}

vector mx_remap(vector in, vector inLow, vector inHigh, vector outLow, vector outHigh, int doClamp)
{
    return vector(mx_remap(in.x, inLow.x, inHigh.x, outLow.x, outHigh.x, doClamp),
                  mx_remap(in.y, inLow.y, inHigh.y, outLow.y, outHigh.y, doClamp),
                  mx_remap(in.z, inLow.z, inHigh.z, outLow.z, outHigh.z, doClamp));
}

vector mx_remap(vector in, float inLow, float inHigh, float outLow, float outHigh, int doClamp)
{
    return vector(mx_remap(in.x, inLow, inHigh, outLow, outHigh, doClamp),
                  mx_remap(in.y, inLow, inHigh, outLow, outHigh, doClamp),
                  mx_remap(in.z, inLow, inHigh, outLow, outHigh, doClamp));
}


//
// Contrast adjustment
//

float mx_contrast(float in, float amount, float pivot)
{
    float out = in - pivot;
    out *= amount;
    out += pivot;
    return out;
}

color mx_contrast(color in, color amount, color pivot)
{
    color out = in - pivot;
    out *= amount;
    out += pivot;
    return out;
}

color mx_contrast(color in, float amount, float pivot)
{
    color out = in - pivot;
    out *= amount;
    out += pivot;
    return out;
}

vector mx_contrast(vector in, vector amount, vector pivot)
{
    return vector(mx_contrast(in.x, amount.x, pivot.x),
                  mx_contrast(in.y, amount.y, pivot.y),
                  mx_contrast(in.z, amount.z, pivot.z));
}

vector mx_contrast(vector in, float amount, float pivot)
{
    return vector(mx_contrast(in.x, amount, pivot),
                  mx_contrast(in.y, amount, pivot),
                  mx_contrast(in.z, amount, pivot));
}

//
// Fractional Brownian Motion (fBm)
//

float mx_fbm(float x, float y, int octaves, float lacunarity, float diminish, string noisetype)
{
    float out = 0;
    float amp = 1.0;
    float xx = x;
    float yy = y;

    for (int i = 0; i < octaves; i += 1) {
        out += amp * noise(noisetype, xx, yy);
        amp *= diminish;
        xx *= lacunarity;
        yy *= lacunarity;
    }
    return out;
}

color mx_fbm(float x, float y, int octaves, float lacunarity, float diminish, string noisetype)
{
    color out = 0;
    float amp = 1.0;
    float xx = x;
    float yy = y;

    for (int i = 0; i < octaves; i += 1) {
        out += amp * (color)noise(noisetype, xx, yy);
        amp *= diminish;
        xx *= lacunarity;
        yy *= lacunarity;
    }
    return out;
}

float mx_fbm(point position, int octaves, float lacunarity, float diminish, string noisetype)
{
    float out = 0;
    float amp = 1.0;
    point p = position;

    for (int i = 0; i < octaves; i += 1) {
        out += amp * noise(noisetype, p);
        amp *= diminish;
        p *= lacunarity;
    }
    return out;
}

color mx_fbm(point position, int octaves, float lacunarity, float diminish, string noisetype)
{
    color out = 0;
    float amp = 1.0;
    point p = position;

    for (int i = 0; i < octaves; i += 1) {
        out += amp * (color)noise(noisetype, p);
        amp *= diminish;
        p *= lacunarity;
    }
    return out;
}


//
// Worley noise helper functions
//

point mx_worley_cell_position(int x, int y, int z, int xoff, int yoff, int zoff, float jitter)
{
    vector off = cellnoise(vector(x+xoff, y+yoff, z+zoff));
    off -= 0.5;
    off *= jitter;
    off += 0.5;
    return point(x, y, z) + off;
}

float mx_worley_distance(point p, int x, int y, int z, int X, int Y, int Z, float jitter, int metric)
{
    point cellpos = mx_worley_cell_position(x, y, z, X, Y, Z, jitter);
    vector diff = cellpos - p;
    
    if (metric == 2)
        return abs(diff[0]) + abs(diff[1]) + abs(diff[2]); // Manhattan distance
    if (metric == 3)
        return max(max(abs(diff[0]), abs(diff[1])), abs(diff[2])); // Chebyshev distance
    return dot(diff, diff); // Euclidean distance squared
}

void mx_sort_distance(float dist, output vector result)
{
    if (dist < result[0])
    {
        result[2] = result[1];
        result[1] = result[0];
        result[0] = dist;
    }
    else if (dist < result[1])
    {
        result[2] = result[1];
        result[1] = dist;
    }
    else if (dist < result[2])
    {
        result[2] = dist;
    }
}

float mx_floorfrac(float x, output int i)
{
    i = (int)floor(x);
    return x - float(i);
}


//
// Worley noise (3D)
//

float mx_worley_noise(vector p, float jitter, int style, int metric)
{
    int X, Y, Z;
    float sqdist = 1e6;
    vector localpos = vector(mx_floorfrac(p.x, X), mx_floorfrac(p.y, Y), mx_floorfrac(p.z, Z));
    vector minpos = vector(0.0, 0.0, 0.0);
    
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            for (int z = -1; z <= 1; ++z)
            {
                float dist = mx_worley_distance(localpos, x, y, z, X, Y, Z, jitter, metric);
                vector cellpos = mx_worley_cell_position(x, y, z, X, Y, Z, jitter) - localpos;
                if(dist < sqdist)
                {
                    sqdist = dist;
                    minpos = cellpos;
                }
            }
        }
    }
    
    if (style == 1)
        return cellnoise(minpos + p);
    else
    {
        if (metric == 0)
            sqdist = sqrt(sqdist);
        return sqdist;
    }
}

vector mx_worley_noise(vector p, float jitter, int style, int metric)
{
    int X, Y, Z;
    vector sqdist = vector(1e6, 1e6, 1e6);
    vector localpos = vector(mx_floorfrac(p.x, X), mx_floorfrac(p.y, Y), mx_floorfrac(p.z, Z));
    vector minpos = vector(0.0, 0.0, 0.0);

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            for (int z = -1; z <= 1; ++z)
            {
                float dist = mx_worley_distance(localpos, x, y, z, X, Y, Z, jitter, metric);
                vector cellpos = mx_worley_cell_position(x, y, z, X, Y, Z, jitter) - localpos;
                if (dist < sqdist.x)
                {
                    sqdist.z = sqdist.y;
                    sqdist.y = sqdist.x;
                    sqdist.x = dist;
                    minpos = cellpos;
                }
                else if (dist < sqdist.y)
                {
                    sqdist.z = sqdist.y;
                    sqdist.y = dist;
                }
                else if (dist < sqdist.z)
                {
                    sqdist.z = dist;
                }
            }
        }
    }
    
    if (style == 1)
        return cellnoise(minpos + p);
    else
    {
        if (metric == 0)
            sqdist = sqrt(sqdist);
        return sqdist;
    }
}

