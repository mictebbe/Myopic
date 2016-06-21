//
//  Copyright 2016 Henrik Tramberend, Hartmut Schirmacher
//

#include "RTR/SceneGraph.hpp"
#include "RTR/WatchThis.hpp"

using namespace ci;

namespace rtr {

const std::string Drawable::surfacePassName = "surface";

/// Creates a new shape from some meshes with a common material that is
/// rendered during the default 'surface' pass.
Shape::Shape(const std::vector<ci::gl::VboMeshRef>& vboMeshes,
             const MaterialRef& material)
  : vboMeshes(vboMeshes)
{
    replaceMaterial(material);
}

Shape::Shape(const std::vector<ci::gl::VboMeshRef>& vboMeshes,
             const MaterialMap& materialMap)
  : vboMeshes(vboMeshes)
{
    setPassMaterials(materialMap);
}

Shape::Shape(
  const std::vector<std::reference_wrapper<const ci::geom::Source>>& sources,
  const MaterialRef& material)
{
    // Use all available atttributes to build the mesh so that shaders can
    // be replaced later without missing any of them. This uses more memory
    // than necessary but is more flexible. And memory is cheap anyway.
    geom::AttribSet allAttributes;
    for (int i = 0; i != geom::Attrib::NUM_ATTRIBS; i++)
        allAttributes.insert(geom::Attrib(i));

    for (const auto& source : sources)
        vboMeshes.push_back(gl::VboMesh::create(source, allAttributes));

    replaceMaterial(material);
}

ShapeRef
Shape::create(const std::vector<ci::gl::VboMeshRef>& vboMeshes,
              const MaterialRef& material)
{
    return std::make_shared<Shape>(vboMeshes, material);
}

ShapeRef
Shape::create(
  const std::vector<std::reference_wrapper<const ci::geom::Source>>& sources,
  const MaterialRef& material)
{
    return std::make_shared<Shape>(sources, material);
}

void
Shape::draw()
{
    draw(surfacePassName);
}

void
Shape::draw(const std::string& pass)
{
    const auto& namedPass = passes.find(pass);
    if (namedPass != passes.end()) {
        const auto& pass = namedPass->second;
        pass.material->bind();
        for (const auto& batch : pass.batches)
            batch->draw();
    }
}

void
Shape::watchMe()
{
    for (const auto& namedPass : passes) {
        watcher.watchForUpdates({ namedPass.second.material });
        watcher.watchForUpdates(namedPass.second.batches);
    }
}

void
Shape::setMaterialForPass(const std::string& passName,
                          const MaterialRef& material)
{
    Pass pass;
    pass.material = material;
    for (const auto& mesh : vboMeshes)
        pass.batches.push_back(gl::Batch::create(mesh, material->program()));

    passes[passName] = pass;
    watchMe();
}

void
Shape::replaceMaterial(const MaterialRef& material)
{
    setPassMaterials({ { surfacePassName, material } });
}

void
Shape::setPassMaterials(const MaterialMap& passMaterials)
{
    passes.clear();

    for (const auto& passMaterial : passMaterials) {
        const auto& name = passMaterial.first;
        const auto& material = passMaterial.second;

        setMaterialForPass(name, material);
    }
    watchMe();
}

void
Shape::replaceProgram(const ci::gl::GlslProgRef& program)
{
    auto& pass = passes[surfacePassName];
    pass.material->replaceProgram(program);
    for (auto batch : pass.batches) {
        batch->replaceGlslProg(pass.material->program());
    }
    watchMe();
}

MaterialRef
Shape::material()
{
    return material(surfacePassName);
}

MaterialRef
Shape::material(const std::string& pass)
{
    const auto& namedPass = passes.find(pass);
    if (namedPass != passes.end()) {
        return namedPass->second.material;
    } else {
        return MaterialRef();
    }
}

Model::Model(const std::vector<ShapeRef>& shapes)
  : shapes(shapes)
{
}

ModelRef
Model::create(const std::vector<ShapeRef>& shapes)
{
    return std::make_shared<Model>(shapes);
}

void
Model::draw()
{
    draw(surfacePassName);
}

void
Model::draw(const std::string& pass)
{
    for (const auto& shape : shapes)
        shape->draw(pass);
}

Node::Node(const std::vector<ModelRef>& models, const glm::mat4& transform,
           const std::vector<NodeRef> children)
  : models(models)
  , transform(transform)
  , children(children)
{
}

NodeRef
Node::create(const std::vector<ModelRef>& models, const glm::mat4& transform,
             const std::vector<NodeRef> children)
{
    return std::make_shared<Node>(models, transform, children);
}

void
Node::draw()
{
    draw(surfacePassName);
}

void
Node::draw(const std::string& pass)
{
    gl::ScopedModelMatrix m;
    gl::multModelMatrix(transform);
    for (auto& model : models)
        model->draw(pass);
    for (auto& child : children)
        child->draw(pass);
}

std::vector<Transformed>
_find(const NodeRef& tree, const NodeRef& node, glm::mat4 worldTransform)
{
    worldTransform *= tree->transform;
    if (tree == node) {
        return { { worldTransform, tree } };
    } else {
        std::vector<Transformed> foundHere;
        for (const auto& child : tree->children) {
            auto foundChild = _find(child, node, worldTransform);
            foundHere.insert(foundHere.end(), foundChild.begin(),
                             foundChild.end());
        }
        return foundHere;
    }
}

std::vector<Transformed>
Node::find(const NodeRef& node)
{
    return _find(shared_from_this(), node, glm::mat4());
}
}
