#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace fs = std::filesystem;

static std::vector<stbrp_rect> image_rects;

struct image {
  std::string name;
  unsigned char* data;
};

static std::vector<image> images;

bool load_image(const fs::path& p) {

  stbrp_rect rect;
  rect.x = 0;
  rect.y = 0;
  rect.was_packed = false;
  unsigned char* data = stbi_load(p.c_str(), reinterpret_cast<int*>(&rect.w), reinterpret_cast<int*>(&rect.h), nullptr, STBI_rgb_alpha);

  if (data == nullptr) {
    std::cerr << "Failed to load: " << p.c_str() << std::endl;
    return false;
  }

  rect.id = images.size();
  images.push_back({p.stem().string(), data});
  image_rects.push_back(rect);

  return true;
}

int main(int argc, char *argv[]) {

  std::string inputDir = argv[1];
  std::string outputDir = argv[2];

  if (outputDir.back() != '/') outputDir += '/';

  std::stringstream strValue;
  strValue << argv[3];

  unsigned atlasW, atlasH;
  strValue >> atlasW;
  strValue.clear();

  strValue << argv[4];
  strValue >> atlasH;

  int channels = 4;

  std::cout << "Creating new atlas with dimensions of " << atlasW << "," << atlasH << std::endl;

  int failCount = 0;
  for (auto& p : fs::recursive_directory_iterator(inputDir)) {
    if (p.is_directory()) continue;
    if (!load_image(p.path()))
      failCount++;
  }

  std::cout << "Loaded " << images.size() << "/" << images.size() + failCount << " images" << std::endl;

  stbrp_context context;
  std::vector<stbrp_node> nodes(images.size());
  stbrp_init_target(&context, atlasW, atlasH, nodes.data(), images.size());
  int success = stbrp_pack_rects(&context, image_rects.data(), image_rects.size());

  if (success == 1)
    std::cout << "Successfully packed all images" << std::endl;
  else {
   failCount = 0;
   for (const auto &r : image_rects) failCount += !r.was_packed;
   std::cerr << "Failed to pack " << failCount << "/" << image_rects.size() << " images" << std::endl;
  }

  unsigned char* atlas = new unsigned char[atlasW*atlasH*channels];
  for (unsigned i = 0; i < atlasW*atlasH*channels; ++i) atlas[i] = 0;

  std::fstream atlasInfo;
  atlasInfo.open(outputDir + "AtlasInfo.txt", std::fstream::out); 

  for (const auto &r : image_rects) {
    if (!r.was_packed) continue;
    for (unsigned y = 0; y < r.h; ++y) {
      for (unsigned x = 0; x < r.w; ++x) {
        unsigned index = channels * (y * r.w + x) ;
        unsigned index_out = channels * ((y + r.y) * atlasW + (x + r.x));
        for (unsigned c = 0; c < channels; ++c) {
          atlas[index_out + c] = images[r.id].data[index + c];
        }
      }
    }
    atlasInfo << '"' << images[r.id].name << '"' << " " << (float)r.x/atlasW  << " " << (float)r.y/atlasH \
      << " " << (float)r.w/atlasW << " " << (float)r.h/atlasH << std::endl;
  }

  success = stbi_write_png((outputDir + "Atlas.png").c_str(), atlasW, atlasH, channels, atlas, atlasW * channels);

  if (success) {
    std::cout << "Wrote atlas image to " << outputDir + "Atlas.png" << std::endl;
  } else {
    std::cerr << "Failed to write atlas image to " << outputDir + "Atlas.png" << std::endl;
  }

  delete[] atlas;

  for (const auto i : images) {
    stbi_image_free(i.data);
  }

  if (!success || failCount > 0) {
   std::cerr << "Failed!" << std::endl;
   return EXIT_FAILURE;
  }

  std::cout << "Success!" << std::endl;
  return EXIT_SUCCESS;
}
