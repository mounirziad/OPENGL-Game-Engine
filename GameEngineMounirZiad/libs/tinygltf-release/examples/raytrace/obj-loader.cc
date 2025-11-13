#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#include "obj-loader.h"

#include "nanort.h"  // for float3

#define TINYOBJLOADER_IMPLEMENTATION
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "stb_image.h"
#include "tiny_obj_loader.h"

#ifdef NANOSG_USE_CXX11
#include <unordered_map>
#else
#include <map>
#endif

#define USE_TEX_CACHE 1

namespace example {

typedef nanort::real3<float> float3;

#ifdef NANOSG_USE_CXX11
static std::unordered_map<std::string, int> hashed_tex;
#else
static std::map<std::string, int> hashed_tex;
#endif

inline void CalcNormal(float3 &N, float3 v0, float3 v1, float3 v2) {
  float3 v10 = v1 - v0;
  float3 v20 = v2 - v0;
  N = vcross(v20, v10);
  N = vnormalize(N);
}

static std::string GetBaseDir(const std::string &filepath) {
  if (filepath.find_last_of("/\\") != std::string::npos)
    return filepath.substr(0, filepath.find_last_of("/\\"));
  return "";
}

static int LoadTexture(const std::string &filename,
                       std::vector<Texture> *textures) {
  if (filename.empty()) return -1;

  // Check cache
  if (USE_TEX_CACHE && hashed_tex.find(filename) != hashed_tex.end())
    return hashed_tex[filename];

  std::cout << "  Loading texture : " << filename << std::endl;
  Texture texture;
  int w, h, n;
  unsigned char *data = stbi_load(filename.c_str(), &w, &h, &n, 0);
  if (!data) {
    std::cout << "  Failed to load : " << filename << std::endl;
    return -1;
  }

  texture.width = w;
  texture.height = h;
  texture.components = n;
  size_t n_elem = size_t(w * h * n);
  texture.image = new unsigned char[n_elem];
  for (size_t i = 0; i < n_elem; i++) texture.image[i] = data[i];

  stbi_image_free(data);

  textures->push_back(texture);
  int idx = int(textures->size()) - 1;

  if (USE_TEX_CACHE) hashed_tex[filename] = idx;

  return idx;
}

static void ComputeBoundingBoxOfMesh(float bmin[3], float bmax[3],
                                     const example::Mesh<float> &mesh) {
  bmin[0] = bmin[1] = bmin[2] = std::numeric_limits<float>::max();
  bmax[0] = bmax[1] = bmax[2] = -std::numeric_limits<float>::max();

  for (size_t i = 0; i < mesh.vertices.size() / 3; i++) {
    bmin[0] = std::min(bmin[0], mesh.vertices[3 * i + 0]);
    bmin[1] = std::min(bmin[1], mesh.vertices[3 * i + 1]);
    bmin[2] = std::min(bmin[2], mesh.vertices[3 * i + 2]);

    bmax[0] = std::max(bmax[0], mesh.vertices[3 * i + 0]);
    bmax[1] = std::max(bmax[1], mesh.vertices[3 * i + 1]);
    bmax[2] = std::max(bmax[2], mesh.vertices[3 * i + 2]);
  }
}

bool LoadObj(const std::string &filename, float scale,
             std::vector<Mesh<float> > *meshes,
             std::vector<Material> *out_materials,
             std::vector<Texture> *out_textures) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  std::string basedir = GetBaseDir(filename) + "/";
  const char *basepath = (basedir == "/") ? nullptr : basedir.c_str();

  // Updated LoadObj call
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                              filename.c_str(), basepath, true);

  if (!warn.empty()) std::cerr << "WARN: " << warn << std::endl;
  if (!err.empty()) std::cerr << "ERR: " << err << std::endl;

  if (!ret) return false;

  std::cout << "[LoadOBJ] # of shapes: " << shapes.size()
            << ", # of materials: " << materials.size() << std::endl;

  // Loop through shapes
  for (size_t i = 0; i < shapes.size(); i++) {
    Mesh<float> mesh(sizeof(float) * 3);
    mesh.name =
        shapes[i].name.empty() ? "_" + std::to_string(i) : shapes[i].name;

    const size_t num_faces = shapes[i].mesh.indices.size() / 3;
    mesh.faces.resize(num_faces * 3);
    mesh.vertices.resize(num_faces * 3 * 3);
    mesh.material_ids.resize(num_faces);
    mesh.facevarying_normals.resize(num_faces * 3 * 3);
    mesh.facevarying_uvs.resize(num_faces * 3 * 2);

    for (size_t f = 0; f < num_faces; f++) {
      int idx0 = shapes[i].mesh.indices[3 * f + 0].vertex_index;
      int idx1 = shapes[i].mesh.indices[3 * f + 1].vertex_index;
      int idx2 = shapes[i].mesh.indices[3 * f + 2].vertex_index;

      mesh.vertices[9 * f + 0] = scale * attrib.vertices[3 * idx0 + 0];
      mesh.vertices[9 * f + 1] = scale * attrib.vertices[3 * idx0 + 1];
      mesh.vertices[9 * f + 2] = scale * attrib.vertices[3 * idx0 + 2];

      mesh.vertices[9 * f + 3] = scale * attrib.vertices[3 * idx1 + 0];
      mesh.vertices[9 * f + 4] = scale * attrib.vertices[3 * idx1 + 1];
      mesh.vertices[9 * f + 5] = scale * attrib.vertices[3 * idx1 + 2];

      mesh.vertices[9 * f + 6] = scale * attrib.vertices[3 * idx2 + 0];
      mesh.vertices[9 * f + 7] = scale * attrib.vertices[3 * idx2 + 1];
      mesh.vertices[9 * f + 8] = scale * attrib.vertices[3 * idx2 + 2];

      mesh.faces[3 * f + 0] = 3 * f + 0;
      mesh.faces[3 * f + 1] = 3 * f + 1;
      mesh.faces[3 * f + 2] = 3 * f + 2;

      mesh.material_ids[f] = shapes[i].mesh.material_ids[f];
    }

    // Normals
    if (!attrib.normals.empty()) {
      for (size_t f = 0; f < num_faces; f++) {
        for (int j = 0; j < 3; j++) {
          int nidx = shapes[i].mesh.indices[3 * f + j].normal_index;
          if (nidx >= 0) {
            mesh.facevarying_normals[3 * (3 * f + j) + 0] =
                attrib.normals[3 * nidx + 0];
            mesh.facevarying_normals[3 * (3 * f + j) + 1] =
                attrib.normals[3 * nidx + 1];
            mesh.facevarying_normals[3 * (3 * f + j) + 2] =
                attrib.normals[3 * nidx + 2];
          }
        }
      }
    }

    // Texcoords
    if (!attrib.texcoords.empty()) {
      for (size_t f = 0; f < num_faces; f++) {
        for (int j = 0; j < 3; j++) {
          int tidx = shapes[i].mesh.indices[3 * f + j].texcoord_index;
          if (tidx >= 0) {
            mesh.facevarying_uvs[2 * (3 * f + j) + 0] =
                attrib.texcoords[2 * tidx + 0];
            mesh.facevarying_uvs[2 * (3 * f + j) + 1] =
                attrib.texcoords[2 * tidx + 1];
          }
        }
      }
    }

    // Compute pivot
    float bmin[3], bmax[3];
    ComputeBoundingBoxOfMesh(bmin, bmax, mesh);
    float bcenter[3] = {0.5f * (bmax[0] + bmin[0]), 0.5f * (bmax[1] + bmin[1]),
                        0.5f * (bmax[2] + bmin[2])};
    for (size_t v = 0; v < mesh.vertices.size() / 3; v++) {
      mesh.vertices[3 * v + 0] -= bcenter[0];
      mesh.vertices[3 * v + 1] -= bcenter[1];
      mesh.vertices[3 * v + 2] -= bcenter[2];
    }

    // Set pivot transform
    mesh.pivot_xform[0][0] = 1.0f;
    mesh.pivot_xform[0][1] = 0.0f;
    mesh.pivot_xform[0][2] = 0.0f;
    mesh.pivot_xform[0][3] = 0.0f;
    mesh.pivot_xform[1][0] = 0.0f;
    mesh.pivot_xform[1][1] = 1.0f;
    mesh.pivot_xform[1][2] = 0.0f;
    mesh.pivot_xform[1][3] = 0.0f;
    mesh.pivot_xform[2][0] = 0.0f;
    mesh.pivot_xform[2][1] = 0.0f;
    mesh.pivot_xform[2][2] = 1.0f;
    mesh.pivot_xform[2][3] = 0.0f;
    mesh.pivot_xform[3][0] = bcenter[0];
    mesh.pivot_xform[3][1] = bcenter[1];
    mesh.pivot_xform[3][2] = bcenter[2];
    mesh.pivot_xform[3][3] = 1.0f;

    meshes->push_back(mesh);
  }

  // Convert materials
  out_materials->resize(materials.size());
  out_textures->clear();
  for (size_t i = 0; i < materials.size(); i++) {
    (*out_materials)[i].diffuse[0] = materials[i].diffuse[0];
    (*out_materials)[i].diffuse[1] = materials[i].diffuse[1];
    (*out_materials)[i].diffuse[2] = materials[i].diffuse[2];
    (*out_materials)[i].specular[0] = materials[i].specular[0];
    (*out_materials)[i].specular[1] = materials[i].specular[1];
    (*out_materials)[i].specular[2] = materials[i].specular[2];
    (*out_materials)[i].id = int(i);

    (*out_materials)[i].diffuse_texid =
        LoadTexture(materials[i].diffuse_texname, out_textures);
    (*out_materials)[i].specular_texid =
        LoadTexture(materials[i].specular_texname, out_textures);
  }

  return true;
}

}  // namespace example
