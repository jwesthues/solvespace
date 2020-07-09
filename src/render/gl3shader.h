//-----------------------------------------------------------------------------
// OpenGL ES 2.0 and OpenGL 3.0 shader interface.
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_GL3SHADER_H
#define SOLVESPACE_GL3SHADER_H

#if defined(WIN32) || defined(__EMSCRIPTEN__)
#   define GL_APICALL /*static linkage*/
#   define GL_GLEXT_PROTOTYPES
#   include <GLES2/gl2.h>
#   include <GLES2/gl2ext.h>
#   define HAVE_GLES
#elif defined(__APPLE__)
#   include <OpenGL/gl.h>
#else
#   define GL_GLEXT_PROTOTYPES
#   include <GL/gl.h>
#   include <GL/glext.h>
#endif

#if !defined(HAVE_GLES)
// glDepthRange is in GL1+ but not GLES2, glDepthRangef is in GL4.1+ and GLES2.
// Consistency!
#   define glClearDepthf glClearDepth
#   define glDepthRangef glDepthRange
#endif

namespace SolveSpace {

//-----------------------------------------------------------------------------
// Floating-point data structures; the layout of these must match shaders
//-----------------------------------------------------------------------------

class Vector2f {
public:
    float x, y;

    static Vector2f From(float x, float y);
    static Vector2f From(double x, double y);
    static Vector2f FromInt(uint32_t x, uint32_t y);
};

class Vector3f {
public:
    float x, y, z;

    static Vector3f From(float x, float y, float z);
    static Vector3f From(const Vector &v);
    static Vector3f From(const RgbaColor &c);
};

class Vector4f {
public:
    float x, y, z, w;

    static Vector4f From(float x, float y, float z, float w);
    static Vector4f From(const Vector &v, float w);
    static Vector4f FromInt(uint32_t x, uint32_t y, uint32_t z, uint32_t w);
    static Vector4f From(const RgbaColor &c);
};

//-----------------------------------------------------------------------------
// Wrappers for our shaders
//-----------------------------------------------------------------------------

class Shader {
public:
    GLuint program = 0;

    void Init(const std::string &vertexRes,
              const std::string &fragmentRes,
              const std::vector<std::pair<GLuint, std::string>> &locations = {});
    void Clear();

    void SetUniformMatrix(const char *name, const double *md);
    void SetUniformVector(const char *name, const Vector &v);
    void SetUniformVector(const char *name, const Vector4f &v);
    void SetUniformColor(const char *name, RgbaColor c);
    void SetUniformFloat(const char *name, float v);
    void SetUniformInt(const char *name, GLint v);
    void SetUniformTextureUnit(const char *name, GLint index);
    void Enable() const;
    void Disable() const;
};

class MeshRenderer {
public:
    const GLint ATTRIB_POS = 0;
    const GLint ATTRIB_NOR = 1;
    const GLint ATTRIB_COL = 2;

    struct MeshVertex {
        Vector3f    pos;
        Vector3f    nor;
        Vector4f    col;
    };

    struct Handle {
        GLuint      vertexBuffer;
        GLsizei     size;
    };

    Shader  lightShader;
    Shader  fillShader;
    Shader *selectedShader = NULL;

    void Init();
    void Clear();

    Handle Add(const SMesh &m, bool dynamic = false);
    void Remove(const Handle &handle);
    void Draw(const Handle &handle, bool useColors = true, RgbaColor overrideColor = {});
    void Draw(const SMesh &mesh, bool useColors = true, RgbaColor overrideColor = {});

    void SetModelview(double *matrix);
    void SetProjection(double *matrix);

    void UseShaded(const Lighting &lighting);
    void UseFilled(const Canvas::Fill &fill);
};

class StippleAtlas {
public:
    std::vector<GLint> patterns;

    void Init();
    void Clear();

    GLint GetTexture(StipplePattern pattern) const;
    double GetLength(StipplePattern pattern) const;
};

class EdgeRenderer {
public:
    const GLint ATTRIB_POS = 0;
    const GLint ATTRIB_LOC = 1;
    const GLint ATTRIB_TAN = 2;

    struct EdgeVertex {
        Vector3f    pos;
        Vector3f    loc;
        Vector3f    tan;
    };

    struct Handle {
        GLuint      vertexBuffer;
        GLuint      indexBuffer;
        GLsizei     size;
    };

    Shader              shader;

    const StippleAtlas *atlas   = NULL;
    StipplePattern      pattern = StipplePattern::CONTINUOUS;

    void Init(const StippleAtlas *atlas);
    void Clear();

    Handle Add(const SEdgeList &edges, bool dynamic = false);
    void Remove(const Handle &handle);
    void Draw(const Handle &handle);
    void Draw(const SEdgeList &edges);

    void SetModelview(const double *matrix);
    void SetProjection(const double *matrix);
    void SetStroke(const Canvas::Stroke &stroke, double pixel);
};

class OutlineRenderer {
public:
    const GLint ATTRIB_POS = 0;
    const GLint ATTRIB_LOC = 1;
    const GLint ATTRIB_TAN = 2;
    const GLint ATTRIB_NOL = 3;
    const GLint ATTRIB_NOR = 4;

    struct OutlineVertex {
        Vector3f    pos;
        Vector4f    loc;
        Vector3f    tan;
        Vector3f    nol;
        Vector3f    nor;
    };

    struct Handle {
        GLuint      vertexBuffer;
        GLuint      indexBuffer;
        GLsizei     size;
    };

    Shader              shader;

    const StippleAtlas *atlas   = NULL;
    StipplePattern      pattern = StipplePattern::CONTINUOUS;

    void Init(const StippleAtlas *atlas);
    void Clear();

    Handle Add(const SOutlineList &outlines, bool dynamic = false);
    void Remove(const Handle &handle);
    void Draw(const Handle &handle, Canvas::DrawOutlinesAs mode);
    void Draw(const SOutlineList &outlines, Canvas::DrawOutlinesAs mode);

    void SetModelview(const double *matrix);
    void SetProjection(const double *matrix);
    void SetStroke(const Canvas::Stroke &stroke, double pixel);
};

class SIndexedMesh {
public:
    struct Vertex {
        Vector3f    pos;
        Vector2f    tex;
    };

    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;

    void AddPoint(const Vector &p);
    void AddQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d);
    void AddPixmap(const Vector &o, const Vector &u, const Vector &v,
                   const Point2d &ta, const Point2d &tb);

    void Clear();
};

class IndexedMeshRenderer {
public:
    const GLint ATTRIB_POS = 0;
    const GLint ATTRIB_TEX = 1;

    struct Handle {
        GLuint      vertexBuffer;
        GLuint      indexBuffer;
        GLsizei     size;
    };

    Shader  texShader;
    Shader  texaShader;
    Shader  colShader;
    Shader  pointShader;

    Shader *selectedShader = NULL;

    void Init();
    void Clear();

    Handle Add(const SIndexedMesh &m, bool dynamic = false);
    void Remove(const Handle &handle);
    void Draw(const Handle &handle);
    void Draw(const SIndexedMesh &mesh);

    void SetModelview(const double *matrix);
    void SetProjection(const double *matrix);

    bool NeedsTexture() const;

    void UseFilled(const Canvas::Fill &fill);
    void UsePoint(const Canvas::Stroke &stroke, double pixel);
};

}

#endif
