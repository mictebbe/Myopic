//
//  Copyright 2016 Henrik Tramberend, Hartmut Schirmacher
//

#pragma once

#include "RTR/ObjLoader.hpp"

#include <cinder/gl/gl.h>

namespace rtr {

using ShaderSources = std::vector<boost::filesystem::path>;

class WatchThis
{
  public:
    using ShaderSources = std::vector<boost::filesystem::path>;

    WatchThis();

    void checkForAndApplyUpdates();
    void checkForChanges();

    ci::gl::GlslProgRef createWatchedProgram(
      const ShaderSources& shaderSources);
    ci::gl::BatchRef createWatchedBatch(const ci::gl::VboMeshRef& vboMesh,
                                        const ShaderSources& shaderSources);
    ci::gl::BatchRef createWatchedBatch(const ci::geom::Source& geomSource,
                                        const ShaderSources& shaderSources);

    void watchForUpdates(const std::vector<ci::gl::BatchRef>& batch);
    void watchForUpdates(const std::vector<MaterialRef>& material);

  private:
    ci::gl::GlslProgRef loadProgram(const ShaderSources& shaderSources);

    void watch(const ci::gl::GlslProgRef& program,
               const ShaderSources& shaderSources);

    std::map<ShaderSources, std::set<ci::gl::BatchRef>> watchedBatches;
    std::map<ShaderSources, std::set<MaterialRef>> watchedMaterials;
    std::map<ShaderSources, ci::gl::GlslProgRef> watchedPrograms;

    std::map<ci::gl::GlslProgRef, ShaderSources> programSources;
    std::map<boost::filesystem::path, ShaderSources> sources;
    std::map<boost::filesystem::path, std::time_t> lastWrite;

    ci::gl::GlslProgRef defaultProgram();
    ci::gl::GlslProgRef defaultProgram_;
};

extern WatchThis watcher;
}
