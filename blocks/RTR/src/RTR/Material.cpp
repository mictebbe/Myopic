//
//  Copyright 2016 Henrik Tramberend, Hartmut Schirmacher
//

#include "RTR/Material.hpp"
#include "RTR/WatchThis.hpp"

using namespace ci;

namespace rtr {

Material::Material(const gl::GlslProgRef& program)
{
    replaceProgram(program);
}

Material::Material(const std::string& name, const gl::GlslProgRef& program)
  : name(name)
{
    replaceProgram(program);
}

void
Material::texture(const std::string& name,
                  const ci::gl::TextureBaseRef& texture)
{
    textures[name] = texture;
}

void
Material::bind()
{
    program_->bind();

    for (auto& parameter : parameters) {
        // Discard uniforms that are not active on the shader
        if (activeUniforms.find(parameter.first) != activeUniforms.end())
            parameter.second->apply(program_, parameter.first);
    }

    int unit = 0;
    for (auto& texture : textures) {
        // Discard uniforms that are not active on the shader
        if (activeUniforms.find(texture.first) != activeUniforms.end()) {
            texture.second->bind(unit);
            program_->uniform(texture.first, unit);
            unit++;
        }
    }
}

void
Material::replaceProgram(const ci::gl::GlslProgRef& program)
{
    program_ = program;
    activeUniforms.clear();
    auto uniformsInfo = program_->getActiveUniforms();
    for (const auto& info : uniformsInfo) {
        activeUniforms.insert(info.getName());
    }
}

MaterialRef
Material::create(const gl::GlslProgRef& program)
{
    return create("", program);
}

MaterialRef
Material::create(const std::string& name, const gl::GlslProgRef& program)
{
    auto material = std::make_shared<Material>(program);
    watcher.watchForUpdates({ material });
    return material;
}
}