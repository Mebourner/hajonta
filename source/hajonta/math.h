#pragma once

struct v2
{
    float x;
    float y;
};

struct v2i
{
    int x;
    int y;
};

struct v3
{
    float x;
    float y;
    float z;
};

union v3i
{
    struct
    {
        int32_t x;
        int32_t y;
        int32_t z;
    };
    int32_t E[3];
};

struct v4b
{
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t w;
};

struct v4i
{
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t w;
};

struct triangle2
{
    v2 p0;
    v2 p1;
    v2 p2;
};

struct triangle3
{
    v3 p0;
    v3 p1;
    v3 p2;
};

struct rectangle2
{
    v2 position;
    v2 dimension;
};

struct line2
{
    v2 position;
    v2 direction;
};

union v4
{
    struct {
        union {
            float x;
            float r;
        };
        union {
            float y;
            float g;
        };
        union {
            float z;
            float b;
        };
        union {
            float w;
            float a;
        };
    };
    float E[4];
};

struct
Quaternion
{
    float x;
    float y;
    float z;
    float w;
};

union m4
{
    v4 cols[4];
    struct
    {
        v4 x;
        v4 y;
        v4 z;
        v4 w;
    };
};

struct
Plane
{
    v3 normal;
    float distance;
};

struct
Ray
{
    v3 location;
    v3 direction;
};
