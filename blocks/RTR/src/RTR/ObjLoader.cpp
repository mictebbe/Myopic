//
//  Copyright 2016 Henrik Tramberend, Hartmut Schirmacher
//

#include "RTR/ObjLoader.hpp"
#include "RTR/tiny_obj_loader.h"
#include "cinder/GeomIo.h"
#include "cinder/Log.h"

#include "glm/ext.hpp"

using namespace ci;
using namespace std;
using namespace glm;

namespace rtr {

static gl::GlslProgRef objShader;

gl::GlslProgRef
defaultObjShader()
{
    if (!objShader) {
        objShader = gl::GlslProg::create(
          gl::GlslProg::Format()
            .vertex(CI_GLSL(
              150, uniform mat4 ciModelViewProjection; in vec4 ciPosition;
              in vec2 ciTexCoord0; out vec2 TexCoord0;

              void main(void) {
                  gl_Position = ciModelViewProjection * ciPosition;
                  TexCoord0 = ciTexCoord0;
              }))
            .fragment(CI_GLSL(
              150, uniform vec3 ka; uniform vec3 kd; uniform sampler2D map_ka;
              uniform sampler2D map_kd; in vec2 TexCoord0; out vec4 oColor;

              vec3 ambient =
                ka.x < 0.0 ? -ka * texture(map_ka, TexCoord0).rgb : ka;
              vec3 diffuse =
                kd.x < 0.0 ? -kd * texture(map_kd, TexCoord0).rgb : kd;

              void main(void) { oColor = vec4(ambient + diffuse, 1); })));
    }
    return objShader;
}

map<fs::path, gl::TextureBaseRef> textureCache;

gl::TextureBaseRef
getTexture(fs::path file)
{
    auto texture = textureCache.find(file);
    if (texture == textureCache.end()) {
        auto image = loadImage(file);
        auto newTexture = gl::Texture2d::create(image);
        textureCache[file] = newTexture;
        // CI_LOG_I("ObjLoader: 2d texture loaded: " << file);
        return newTexture;
    } else {
        return texture->second;
    }
}

void
normalizePositions(std::vector<tinyobj::shape_t>& shapes)
{
    auto mf = std::numeric_limits<float>::max();
    vec3 minPos(mf, mf, mf);
    vec3 maxPos(-mf, -mf, -mf);

    for (auto& shape : shapes) {
        auto& pos = shape.mesh.positions;
        for (int i = 0; i < pos.size() / 3; i++) {
            auto vertex = vec3(pos[i * 3 + 0], pos[i * 3 + 1], pos[i * 3 + 2]);
            minPos = glm::min(minPos, vertex);
            maxPos = glm::max(maxPos, vertex);
        }
    }

    auto center = (minPos + maxPos) / 2.0f;
    auto extend = maxPos - minPos;
    auto scale = 2.0f / std::max(std::max(extend.x, extend.y), extend.z);
    auto offset = -center;

    for (auto& shape : shapes) {
        auto& pos = shape.mesh.positions;
        for (int i = 0; i < pos.size() / 3; i++) {
            auto vertex = vec3(pos[i * 3 + 0], pos[i * 3 + 1], pos[i * 3 + 2]);
            vertex = (vertex + offset) * scale;
            pos[i * 3 + 0] = vertex.x;
            pos[i * 3 + 1] = vertex.y;
            pos[i * 3 + 2] = vertex.z;
        }
    }
}

// Lifted from cinder/GeomIo.hpp with a crahing bug fixed.
class Tangents : public geom::Modifier
{
  public:
    Tangents() {}

    uint8_t getAttribDims(geom::Attrib attr,
                          uint8_t upstreamDims) const override;
    geom::AttribSet getAvailableAttribs(
      const Modifier::Params& upstreamParams) const override;

    Modifier* clone() const override { return new Tangents; }
    void process(geom::SourceModsContext* ctx,
                 const geom::AttribSet& requestedAttribs) const override;
};

ModelRef
loadObjFile(const fs::path& file, bool normalize, const gl::GlslProgRef& shader)
{
    auto basePath = fs::absolute(file.parent_path());

    std::vector<ShapeRef> bins;

    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string err;
    if (!tinyobj::LoadObj(shapes, materials, err, file.string().c_str(),
                          (basePath.string() + "/").c_str())) {
        throw Exception("ObjLoader: error loading: " + file.string() + ": " +
                        err);
    }
    if (!err.empty())
        CI_LOG_W("ObjLoader: " << err);

    if (normalize)
        normalizePositions(shapes);

    vector<MaterialRef> materialLib;
    for (const auto& mat : materials) {
        auto material =
          Material::create(mat.name, shader ? shader : defaultObjShader());
        if (mat.ambient_texname.empty()) {
            material->uniform("ka", glm::make_vec3(mat.ambient));
        } else {
            material->uniform("ka", -glm::make_vec3(mat.ambient));
            material->texture("map_ka",
                              getTexture(basePath / mat.ambient_texname));
        }
        if (mat.diffuse_texname.empty()) {
            material->uniform("kd", glm::make_vec3(mat.diffuse));
        } else {
            material->uniform("kd", -glm::make_vec3(mat.diffuse));
            material->texture("map_kd",
                              getTexture(basePath / mat.diffuse_texname));
        }
        if (mat.specular_texname.empty()) {
            material->uniform("ks", glm::make_vec3(mat.specular));
        } else {
            material->uniform("ks", -glm::make_vec3(mat.specular));
            material->texture("map_ks",
                              getTexture(basePath / mat.specular_texname));
        }
        if (mat.specular_highlight_texname.empty()) {
            material->uniform("ns", float(mat.shininess));
        } else {
            material->uniform("ns", -float(mat.shininess));
            material->texture(
              "map_ns", getTexture(basePath / mat.specular_highlight_texname));
        }
        if (!mat.bump_texname.empty()) {
            material->texture("map_bump",
                              getTexture(basePath / mat.bump_texname));
        }
        if (!mat.displacement_texname.empty()) {
            material->texture("disp",
                              getTexture(basePath / mat.displacement_texname));
        }
        if (!mat.alpha_texname.empty()) {
            material->texture("map_d",
                              getTexture(basePath / mat.alpha_texname));
        }
        materialLib.push_back(material);
    }

    // Use all available atttributes to build the mesh so that shaders can
    // be replaced later without missing any of them. This uses more memory
    // than necessary but is more flexible. And memory is cheap anyway.
    geom::AttribSet allAttributes;
    for (int i = 0; i != geom::Attrib::NUM_ATTRIBS; i++)
        allAttributes.insert(geom::Attrib(i));

    for (const auto& s : shapes) {
        const auto& mesh = s.mesh;

        TriMesh::Format meshFormat;
        meshFormat.positions();
        if (mesh.normals.size())
            meshFormat.normals();
        if (mesh.texcoords.size())
            meshFormat.texCoords();
        if (mesh.normals.size() && mesh.texcoords.size())
            meshFormat.tangents().bitangents();

        TriMesh triMesh(meshFormat);

        triMesh.appendPositions(
          reinterpret_cast<const vec3*>(&mesh.positions[0]),
          mesh.positions.size() / 3);

        if (mesh.normals.size())
            triMesh.appendNormals(
              reinterpret_cast<const vec3*>(&mesh.normals[0]),
              mesh.normals.size() / 3);

        if (mesh.texcoords.size())
            triMesh.appendTexCoords0(
              reinterpret_cast<const vec2*>(&mesh.texcoords[0]),
              mesh.texcoords.size() / 2);

        for (size_t i = 0; i < mesh.indices.size(); i += 3)
            triMesh.appendTriangle(mesh.indices[i + 0], mesh.indices[i + 1],
                                   mesh.indices[i + 2]);

        // TODO Support per face materials. For now, use the material of the
        // first face for the entire mesh. Warn, if this assumption is not true.
        bool homogenous = true;
        for (auto id : mesh.material_ids)
            homogenous &= id == mesh.material_ids[0];
        if (!homogenous)
            CI_LOG_W("Per-face materials detected and ignored");

        auto& material = materialLib[mesh.material_ids[0]];
        if (triMesh.hasNormals() && triMesh.hasTexCoords()) {
            auto withTangents = triMesh >> Tangents();
            bins.push_back(Shape::create(
              { gl::VboMesh::create(withTangents, allAttributes) }, material));
        } else {
            bins.push_back(Shape::create(
              { gl::VboMesh::create(triMesh, allAttributes) }, material));
        }
    }

    return Model::create(bins);
}

using namespace geom;

uint8_t
Tangents::getAttribDims(Attrib attr, uint8_t upstreamDims) const
{
    if (attr == Attrib::TANGENT || attr == Attrib::BITANGENT)
        return 3;
    else
        return upstreamDims;
}

AttribSet
Tangents::getAvailableAttribs(const Modifier::Params& upstreamParams) const
{
    return { Attrib::TANGENT, Attrib::BITANGENT };
}

void
Tangents::process(SourceModsContext* ctx,
                  const AttribSet& requestedAttribs) const
{
    AttribSet request = requestedAttribs;
    request.insert(Attrib::POSITION);
    request.insert(Attrib::NORMAL);
    request.insert(Attrib::TEX_COORD_0);
    ctx->processUpstream(request);

    const size_t numIndices = ctx->getNumIndices();
    const size_t numVertices = ctx->getNumVertices();

    if (numIndices == 0) {
        CI_LOG_W("geom::Tangents requires indexed geometry");
        return;
    }
    if (ctx->getAttribDims(Attrib::POSITION) != 3) {
        CI_LOG_W("geom::Tangents requires 3D positions");
        return;
    }
    if (ctx->getAttribDims(Attrib::NORMAL) != 3) {
        CI_LOG_W("geom::Tangents requires 3D normals");
        return;
    }
    if (ctx->getAttribDims(Attrib::TEX_COORD_0) != 2) {
        CI_LOG_W("geom::Tangents requires 2D texture coordinates");
        return;
    }

    const vec3* positions = (const vec3*)ctx->getAttribData(geom::POSITION);
    const vec3* normals = (const vec3*)ctx->getAttribData(geom::NORMAL);
    const vec2* texCoords = (const vec2*)ctx->getAttribData(geom::TEX_COORD_0);

    if (requestedAttribs.count(geom::TANGENT) ||
        requestedAttribs.count(geom::BITANGENT)) {
        vector<vec3> tangents, bitangents;
        calculateTangents(numIndices, ctx->getIndicesData(), numVertices,
                          positions, normals, texCoords, &tangents,
                          &bitangents);

        if (requestedAttribs.count(geom::TANGENT))
            ctx->copyAttrib(Attrib::TANGENT, 3, 0,
                            (const float*)tangents.data(), numVertices);
        if (requestedAttribs.count(geom::BITANGENT))
            ctx->copyAttrib(Attrib::BITANGENT, 3, 0,
                            (const float*)bitangents.data(), numVertices);
    }
}
}
