//
//  Copyright 2016 Henrik Tramberend, Hartmut Schirmacher
//

#pragma once

#include "RTR/Material.hpp"
#include "cinder/gl/gl.h"

#include <memory>

namespace rtr {

class WatchThis;

class Drawable;
using DrawableRef = std::shared_ptr<Drawable>;

class Drawable
{
  public:
    // Unfortunately, overriding material or shader program on the fly does
    // not make sense. The batches needed to render an ad hoc combination of
    // mesh attributes and shader attributes just do not exist.
    //
    // Instead, we need the concept of a rendering pass, where a shape is
    // associated with multiple materials, one for each supported pass. A
    // shape that lacks a material for a pass, ist not drawn during that pass.
    //
    // The current pass is identified by name and passed to the draw() function.
    //
    // So instead of
    //
    //   virtual void draw(const MaterialRef& material) = 0;
    //   virtual void draw(const ci::gl::GlslProgRef& program) = 0;
    //
    // the second draw funtion will be
    //
    //   virtual void draw(const std::string& pass) = 0;
    //

    /// The name of the default rendering pass.
    static const std::string surfacePassName;

    /// Draw the shape geometry for the identified pass.
    virtual void draw(const std::string& pass) = 0;

    /// Draw the shape geometry for the default rendering pass.
    virtual void draw() = 0;
};

class Shape;
using ShapeRef = std::shared_ptr<Shape>;

class Shape : public Drawable
{
  public:
    Shape(const std::vector<ci::gl::VboMeshRef>& vboMeshes,
          const MaterialRef& material);
    Shape(const std::vector<ci::gl::VboMeshRef>& vboMeshes,
          const MaterialMap& materialMap);
    Shape(const std::vector<std::reference_wrapper<const ci::geom::Source>>&
            sources,
          const MaterialRef& material);

    static ShapeRef create(const std::vector<ci::gl::VboMeshRef>& vboMeshes,
                           const MaterialRef& material);
    static ShapeRef create(
      const std::vector<std::reference_wrapper<const ci::geom::Source>>&
        sources,
      const MaterialRef& material);

    void draw() override;
    void draw(const std::string& pass) override;

    void setMaterialForPass(const std::string& pass,
                            const MaterialRef& material);
    void setPassMaterials(const MaterialMap& passMaterials);

    void replaceMaterial(const MaterialRef& material);
    void replaceProgram(const ci::gl::GlslProgRef& program);

    MaterialRef material();
    MaterialRef material(const std::string& pass);

  private:
    void watchMe();

    std::vector<ci::gl::VboMeshRef> vboMeshes;

    struct Pass
    {
        MaterialRef material;
        std::vector<ci::gl::BatchRef> batches;
    };

    using PassSet = std::map<std::string, Pass>;

    PassSet passes;
};

class Model;
using ModelRef = std::shared_ptr<Model>;

class Model : public Drawable
{
  public:
    Model(const std::vector<ShapeRef>& shapes);

    static ModelRef create(const std::vector<ShapeRef>& shapes);

    void draw() override;
    void draw(const std::string& pass) override;

    std::vector<ShapeRef> shapes;
};

class Node;
using NodeRef = std::shared_ptr<Node>;

struct Transformed
{
    glm::mat4 transform;
    NodeRef node;
};

class Node : public Drawable, public std::enable_shared_from_this<Node>
{
  public:
    Node(const std::vector<ModelRef>& models, const glm::mat4& transform,
         const std::vector<NodeRef> children);

    static NodeRef create(
      const std::vector<ModelRef>& models = std::vector<ModelRef>(),
      const glm::mat4& transform = glm::mat4(),
      const std::vector<NodeRef> children = std::vector<NodeRef>());

    void draw() override;
    void draw(const std::string& pass) override;

    std::vector<Transformed> find(const NodeRef& node);

    std::vector<ModelRef> models;
    glm::mat4 transform;
    std::vector<NodeRef> children;
};
}
