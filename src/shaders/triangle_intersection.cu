
#include <optix.h>
#include <optixu/optixu_math_namespace.h>

#include "../core/vertexattributes.h"
#include "../math/basic.h"

rtBuffer<VertexAttributes> attributesBuffer;

// Attributes.
rtDeclareVariable(optix::float3, varGeoNormal, attribute GEO_NORMAL, );
rtDeclareVariable(optix::float3, varTangent,   attribute TANGENT, );
rtDeclareVariable(optix::float3, varNormal,    attribute NORMAL, );
rtDeclareVariable(optix::float3, varTexCoord,  attribute TEXCOORD, );

rtDeclareVariable(optix::Ray, theRay, rtCurrentRay, );

RT_PROGRAM void triangle_intersection(int primitiveIndex)
{
    VertexAttributes const& a0 = attributesBuffer[3*primitiveIndex  ];
    VertexAttributes const& a1 = attributesBuffer[3*primitiveIndex+1];
    VertexAttributes const& a2 = attributesBuffer[3*primitiveIndex+2];

    float3 n;
    float  t;
    float  beta;
    float  gamma;

    if (intersect_triangle(theRay, a0.vertex, a1.vertex, a2.vertex, n, t, beta, gamma))
    {
        if (rtPotentialIntersection(t))
        {
            // Barycentric interpolation:
            const float alpha = 1.0f - beta - gamma;

            // Note: No normalization on the TBN attributes here for performance reasons.
            //       It's done after the transformation into world space anyway.
            varGeoNormal = n;

            if (isNull(a0.tangent)){
                float x1 = a1.vertex.x - a0.vertex.x;
                float x2 = a2.vertex.x - a0.vertex.x;
                float y1 = a1.vertex.y - a0.vertex.y;
                float y2 = a2.vertex.y - a0.vertex.y;
                float z1 = a1.vertex.z - a0.vertex.z;
                float z2 = a2.vertex.z - a0.vertex.z;

                float s1 = a1.texcoord.x - a0.texcoord.x;
                float s2 = a2.texcoord.x - a0.texcoord.x;
                float t1 = a1.texcoord.y - a0.texcoord.y;
                float t2 = a2.texcoord.y - a0.texcoord.y;

                float r = 1.f / (s1 * t2 - s2 * t1);
                varTangent = make_float3((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
                              (t2 * z1 - t1 * z2) * r);
//                float3 tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
//                              (s1 * z2 - s2 * z1) * r);
            }
            else
                varTangent = a0.tangent  * alpha + a1.tangent  * beta + a2.tangent  * gamma;

            if (isNull(a0.normal))
                varNormal = varGeoNormal;
            else
                varNormal = a0.normal   * alpha + a1.normal   * beta + a2.normal   * gamma;

            varTexCoord = a0.texcoord * alpha + a1.texcoord * beta + a2.texcoord * gamma;

            rtReportIntersection(0);
        }
    }
}