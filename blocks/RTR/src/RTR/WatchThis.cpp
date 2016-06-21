//
//  Copyright 2016 Henrik Tramberend, Hartmut Schirmacher
//

#include "RTR/WatchThis.hpp"
#include "cinder/Log.h"

using namespace ci;
using namespace std;

namespace rtr {

WatchThis watcher;

// Constructor creates a default checkerboard shader.
WatchThis::WatchThis()
{
}

ci::gl::GlslProgRef
WatchThis::defaultProgram()
{
    if (!defaultProgram_) {
        defaultProgram_ = gl::GlslProg::create(
          gl::GlslProg::Format()
            .vertex(CI_GLSL(
              150, uniform mat4 ciModelViewProjection; in vec4 ciPosition;
              in vec2 ciTexCoord0; out vec2 TexCoord0;

              void main(void) {
                  gl_Position = ciModelViewProjection * ciPosition;
                  TexCoord0 = ciTexCoord0;
              }))
            .fragment(CI_GLSL(
              150, const float uCheckSize = 8;

              in vec2 TexCoord0; out vec4 oColor;

              vec4 checker(vec2 uv) {
                  float v = floor(uCheckSize * uv.x) + floor(uCheckSize * uv.y);
                  if (mod(v, 2.0) < 1.0)
                      return vec4(0.8, 0.8, 0.8, 1);
                  else
                      return vec4(0.4, 0.4, 0.4, 1);
              }

              void main(void) { oColor = checker(TexCoord0); })));
    }
    return defaultProgram_;
}

void
WatchThis::checkForChanges()
{
    std::set<ShaderSources> changed;
    for (auto& fileAndWriteTime : lastWrite) {
        auto& file = fileAndWriteTime.first;
        auto& time = fileAndWriteTime.second;

        boost::system::error_code error;
        auto lastWrite = fs::last_write_time(file, error);

        if (lastWrite > time) {
            changed.insert(sources[file]);
            time = lastWrite;
        }
    }

    for (auto& sources : changed) {
        auto reloaded = loadProgram(sources);
        watchedPrograms[sources] = reloaded;

        auto sourcesAndBatch = watchedBatches.find(sources);
        if (sourcesAndBatch != watchedBatches.end()) {
            for (auto& batch : sourcesAndBatch->second) {
                batch->replaceGlslProg(reloaded);
            }
        }
        auto sourcesAndMaterial = watchedMaterials.find(sources);
        if (sourcesAndMaterial != watchedMaterials.end()) {
            for (auto& material : sourcesAndMaterial->second) {
                material->replaceProgram(reloaded);
            }
        }
    }
}

gl::GlslProgRef
WatchThis::loadProgram(const ShaderSources& shaderSources)
{
    gl::GlslProgRef program;

    try {
        std::array<DataSourceRef, 5> sources;

        int count = 0;
        for (auto& filepath : shaderSources) {
            sources[count++] = loadFile(filepath);
        }
        program = gl::GlslProg::create(sources[0], sources[1], sources[2],
                                       sources[3], sources[4]);
    } catch (cinder::Exception& e) {
        CI_LOG_E(e.what());
        program = defaultProgram();
    }

    return program;
}

void
WatchThis::watchForUpdates(const std::vector<ci::gl::BatchRef>& batches)
{
    for (auto& batch : batches) {
        auto& batchProgram = batch->getGlslProg();
        auto sources = programSources.find(batchProgram);
        if (sources != programSources.end()) {
            watchedBatches[sources->second].insert(batch);
        } else {
            // TODO log warning
        }
    }
}

void
WatchThis::watchForUpdates(const std::vector<MaterialRef>& materials)
{
    for (auto& material : materials) {
        auto& materialProgram = material->program();
        auto sources = programSources.find(materialProgram);
        if (sources != programSources.end()) {
            watchedMaterials[sources->second].insert(material);
        } else {
            // TODO log warning
        }
    }
}

ci::gl::GlslProgRef
WatchThis::createWatchedProgram(const ShaderSources& shaderSources)
{
    auto existing = watchedPrograms.find(shaderSources);
    if (existing != watchedPrograms.end()) {
        return existing->second;
    } else {
        auto program = loadProgram(shaderSources);
        watchedPrograms[shaderSources] = program;
        programSources[program] = shaderSources;
        for (const auto& filepath : shaderSources) {
            if (!filepath.empty()) {
                sources[filepath] = shaderSources;
                boost::system::error_code error;
                lastWrite[filepath] = fs::last_write_time(filepath, error);
                CI_LOG_I("Now watching: " << filepath.filename());
            }
        }
        return program;
    }
}

gl::BatchRef
WatchThis::createWatchedBatch(const gl::VboMeshRef& vboMesh,
                              const ShaderSources& shaderSources)
{
    auto program = createWatchedProgram(shaderSources);
    auto batch = gl::Batch::create(vboMesh, program);
    watchedBatches[shaderSources].insert(batch);
    return batch;
}

gl::BatchRef
WatchThis::createWatchedBatch(const geom::Source& geomSource,
                              const ShaderSources& shaderSources)
{
    // Use all available atttributes to build the mesh so that shaders can
    // be replaced later without missing any of them. This uses more memory
    // than necessary but is more flexible. And memory is cheap anyway.
    geom::AttribSet allAttributes;
    for (int i = 0; i != geom::Attrib::NUM_ATTRIBS; i++)
        allAttributes.insert(geom::Attrib(i));

    auto program = createWatchedProgram(shaderSources);
    auto batch = gl::Batch::create(
      gl::VboMesh::create(geomSource, allAttributes), program);
    watchedBatches[shaderSources].insert(batch);
    return batch;
}
}