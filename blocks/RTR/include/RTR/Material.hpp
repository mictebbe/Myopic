//
//  Copyright 2016 Henrik Tramberend, Hartmut Schirmacher
//

#pragma once

#include "cinder/gl/gl.h"

namespace rtr {

///
/// \brief The UniformValue class represents a uniform value. The uniform
/// value can be of any type that is supported by the uniform() method of the
/// program class.
///
class UniformValue
{
  public:
    using Ptr = std::shared_ptr<UniformValue>;
    using Map = std::map<std::string, UniformValue::Ptr>;

    virtual void apply(ci::gl::GlslProgRef& program,
                       const std::string name) = 0;
};

///
/// \brief A generic uniform value.
///
template <typename T>
class UValue : public UniformValue
{
  public:
    using Ptr = std::shared_ptr<UValue>;

    T value;
    UValue(const T& v)
      : value(v)
    {
    }

    void apply(ci::gl::GlslProgRef& program, const std::string name)
    {
        program->uniform(name, value);
    }
};

///
/// \brief The Material class represents a parameterized shader
/// program.
/// Parameters are set as uniform variables and textures on the shader program
/// when
/// the material is bound.
///
class Material;
using MaterialRef = std::shared_ptr<Material>;
using MaterialMap = std::map<std::string, MaterialRef>;

class Material
{
  public:
    Material(const ci::gl::GlslProgRef& program);
    Material(const std::string& name, const ci::gl::GlslProgRef& program);

    // TODO (constructor and ObjLoader)
    std::string name;

    /// \brief Uses the program associated with this material for subsequent
    /// rendering
    /// and sets the uniform variables according to the material parameters.
    void bind();

    /// \brief Sets the named material parameter to the provided value. The
    /// named parameter
    /// is bound to a uniform variable of the same name in the associated shader
    /// program.
    template <typename T>
    void uniform(const std::string& name, const T& v)
    {
        parameters[name] = std::make_shared<UValue<T>>(v);
    }

    void texture(const std::string& name,
                 const ci::gl::TextureBaseRef& texture);

    void printActiveUniforms()
    {
        for (const auto& au : activeUniforms)
            std::cout << au << " ";
        std::cout << std::endl;
    }

    /// \brief Creates a new generic material with no associated shader program.
    static MaterialRef create(const ci::gl::GlslProgRef& program);
    static MaterialRef create(const std::string& name,
                              const ci::gl::GlslProgRef& program);

  private:
    using Map = std::map<std::string, MaterialRef>;
    using TexturesMap = std::map<std::string, ci::gl::TextureBaseRef>;

    friend class Shape;
    friend class WatchThis;

    const ci::gl::GlslProgRef program() const { return program_; }
    void replaceProgram(const ci::gl::GlslProgRef& program);

    ci::gl::GlslProgRef program_;
    std::set<std::string> activeUniforms;

    UniformValue::Map parameters;
    TexturesMap textures;
};
}
