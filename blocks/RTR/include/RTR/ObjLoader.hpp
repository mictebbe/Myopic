//
//  Copyright 2016 Henrik Tramberend, Hartmut Schirmacher
//

#pragma once

#include "RTR/SceneGraph.hpp"
#include "cinder/gl/gl.h"

namespace rtr {

/**
 * \brief Loads a Wavefront OBJ file from the file system and returns a
 * rtr::Model object.
 */
ModelRef loadObjFile(const boost::filesystem::path& file, bool normalize = true,
                     const ci::gl::GlslProgRef& shader = ci::gl::GlslProgRef());
}
