#include "hash.h"
#include "util.h"
#include "vector.h"
#include <cmath>
#include <queue>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

/* enum class PEEK_FACES : unsigned char {
  FRONT = 0,
  BACK,
  LEFT,
  RIGHT,
  TOP,
  BOTTOM,
  NONE
};
unsigned char PEEK_FACE_INDICES[8 * 8];
void initCull() {
  std::fill_n(PEEK_FACE_INDICES, sizeof(PEEK_FACE_INDICES) / sizeof(PEEK_FACE_INDICES[0]), 0xFF);

  unsigned char peekIndex = 0;
  for (unsigned int i = 0; i < 6; i++) {
    for (unsigned int j = 0; j < 6; j++) {
      if (i != j) {
        const unsigned char otherEntry = PEEK_FACE_INDICES[j << 3 | i];
        PEEK_FACE_INDICES[i << 3 | j] = otherEntry != 0xFF ? otherEntry : peekIndex++;
      }
    }
  }
} */

struct CullQueueEntry {
  int x;
  int y;
  int z;
  int enterFace;
};

struct PeekFace {
  int exitFace;
  int enterFace;
  int x;
  int y;
  int z;
};
PeekFace peekFaceSpecs[] = {
  {(int)PEEK_FACES::BACK, (int)PEEK_FACES::FRONT, 0, 0, -1},
  {(int)PEEK_FACES::FRONT, (int)PEEK_FACES::BACK, 0, 0, 1},
  {(int)PEEK_FACES::LEFT, (int)PEEK_FACES::RIGHT, -1, 0, 0},
  {(int)PEEK_FACES::RIGHT, (int)PEEK_FACES::LEFT, 1, 0, 0},
  {(int)PEEK_FACES::TOP, (int)PEEK_FACES::BOTTOM, 0, 1, 0},
  {(int)PEEK_FACES::BOTTOM, (int)PEEK_FACES::TOP, 0, -1, 0},
};
const unsigned int numPeekFaceSpecs = sizeof(peekFaceSpecs) / sizeof(peekFaceSpecs[0]);

struct MapChunkMesh {
  unsigned char *peeks;
  int landStart;
  int landCount;
  int waterStart;
  int waterCount;
  int lavaStart;
  int lavaCount;
  bool visible;
};

unsigned int cullTerrain(float *hmdPosition, float *projectionMatrix, float *matrixWorldInverse, int *mapChunkMeshes, unsigned int numMapChunkMeshes, int *groups) {
  const int ox = (int)hmdPosition[0] >> 4;
  const int oy = std::min<int>(std::max<int>(std::floor((int)hmdPosition[1] >> 4), 0), NUM_CHUNKS_HEIGHT - 1);
  const int oz = std::floor((int)hmdPosition[2] >> 4);

  std::unordered_map<std::tuple<int, int, int>, MapChunkMesh> mapChunkMeshMap;
  mapChunkMeshMap.reserve(512 * NUM_CELLS_HEIGHT);
  std::unordered_set<std::pair<int, int>> mapChunkMeshSet;
  mapChunkMeshSet.reserve(512 * NUM_CELLS_HEIGHT);
// std::cout << "check num map chunk meshes " << numMapChunkMeshes << "\n";
  for (unsigned int i = 0; i < numMapChunkMeshes; i++) {
    const unsigned int baseIndex = i * 14;
    const bool valid = mapChunkMeshes[baseIndex + 0] > 0;

    if (valid) {
      const int x = mapChunkMeshes[baseIndex + 1];
      const int y = mapChunkMeshes[baseIndex + 2];
      const int z = mapChunkMeshes[baseIndex + 3];
// std::cout << "valid " << x << " : " << y << " : " << z << "\n";
      unsigned char *peeks = (unsigned char *)(mapChunkMeshes + (baseIndex + 4));
      const std::tuple<int, int, int> key(x, y, z);
      mapChunkMeshMap[key] = MapChunkMesh{
        peeks,
        mapChunkMeshes[baseIndex + 8],
        mapChunkMeshes[baseIndex + 9],
        mapChunkMeshes[baseIndex + 10],
        mapChunkMeshes[baseIndex + 11],
        mapChunkMeshes[baseIndex + 12],
        mapChunkMeshes[baseIndex + 13],
        false
      };
      mapChunkMeshSet.emplace(x, z);
    }
  }

  std::queue<CullQueueEntry> cullQueue;

  const std::tuple<int, int, int> key(ox, oy, oz);
  const std::unordered_map<std::tuple<int, int, int>, MapChunkMesh>::iterator trackedMapChunkMesh = mapChunkMeshMap.find(key);
  if (trackedMapChunkMesh != mapChunkMeshMap.end()) {
    const Frustum localFrustum = Frustum::fromMatrix(Matrix::fromArray(projectionMatrix) *= Matrix::fromArray(matrixWorldInverse));

    cullQueue.push(CullQueueEntry{ox, oy, oz, (unsigned char)PEEK_FACES::NONE});
    while (cullQueue.size() > 0) {
      const CullQueueEntry entry = cullQueue.front();
      cullQueue.pop();

      const int x = entry.x;
      const int y = entry.y;
      const int z = entry.z;
      const unsigned char enterFace = entry.enterFace;

      const std::tuple<int, int, int> key(x, y, z);
      MapChunkMesh &trackedMapChunkMesh = mapChunkMeshMap[key];
      trackedMapChunkMesh.visible = true;

      for (unsigned int j = 0; j < numPeekFaceSpecs; j++) {
        const PeekFace &peekFaceSpec = peekFaceSpecs[j];
        const int ay = y + peekFaceSpec.y;
        if (ay >= 0 && ay < NUM_CHUNKS_HEIGHT) {
          const int ax = x + peekFaceSpec.x;
          const int az = z + peekFaceSpec.z;
          if (
            (ax - ox) * peekFaceSpec.x > 0 ||
            (ay - oy) * peekFaceSpec.y > 0 ||
            (az - oz) * peekFaceSpec.z > 0
          ) {
            if (enterFace == (int)PEEK_FACES::NONE || trackedMapChunkMesh.peeks[PEEK_FACE_INDICES[enterFace << 3 | peekFaceSpec.exitFace]] == 1) {
              Sphere boundingSphere(
                ax * NUM_CELLS + NUM_CELLS_HALF,
                ay * NUM_CELLS + NUM_CELLS_HALF,
                az * NUM_CELLS + NUM_CELLS_HALF,
                NUM_CELLS_CUBE
              );
              if (localFrustum.intersectsSphere(boundingSphere)) {
                cullQueue.push(CullQueueEntry{ax, ay, az, peekFaceSpec.enterFace});
              }
            }
          }
        }
      }
    }
  }

  unsigned int groupIndex = 0;
  for (auto const &iter : mapChunkMeshSet) {
    int x = iter.first;
    int z = iter.second;

    groups[groupIndex++] = getChunkIndex(x, z);
    for (int i = 0; i < NUM_RENDER_GROUPS * 6; i++) {
      groups[groupIndex + i] = -1;
    }

    int landGroupIndex = 0;
    int landStart = -1;
    int landCount = 0;
    int waterGroupIndex = 0;
    int waterStart = -1;
    int waterCount = 0;
    int lavaGroupIndex = 0;
    int lavaStart = -1;
    int lavaCount = 0;

    for (int i = 0; i < NUM_CHUNKS_HEIGHT; i++) {
      const std::tuple<int, int, int> key(x, i, z);
      const MapChunkMesh &trackedMapChunkMesh = mapChunkMeshMap[key];
      if (trackedMapChunkMesh.visible) {
        if (landStart == -1 && trackedMapChunkMesh.landCount > 0) {
          landStart = trackedMapChunkMesh.landStart;
        }
        landCount += trackedMapChunkMesh.landCount;

        if (waterStart == -1 && trackedMapChunkMesh.waterCount > 0) {
          waterStart = trackedMapChunkMesh.waterStart;
        }
        waterCount += trackedMapChunkMesh.waterCount;

        if (lavaStart == -1 && trackedMapChunkMesh.lavaCount > 0) {
          lavaStart = trackedMapChunkMesh.lavaStart;
        }
        lavaCount += trackedMapChunkMesh.lavaCount;
      } else {
        if (landStart != -1) {
          const int baseIndex = groupIndex + landGroupIndex * 6;
          groups[baseIndex + 0] = landStart;
          groups[baseIndex + 1] = landCount;
          landGroupIndex++;
          landStart = -1;
          landCount = 0;
        }
        if (waterStart != -1) {
          const int baseIndex = groupIndex + waterGroupIndex * 6;
          groups[baseIndex + 2] = waterStart;
          groups[baseIndex + 3] = waterCount;
          waterGroupIndex++;
          waterStart = -1;
          waterCount = 0;
        }
        if (lavaStart != -1) {
          const int baseIndex = groupIndex + lavaGroupIndex * 6;
          groups[baseIndex + 4] = lavaStart;
          groups[baseIndex + 5] = lavaCount;
          lavaGroupIndex++;
          lavaStart = -1;
          lavaCount = 0;
        }
      }
    }
    if (landStart != -1) {
      const int baseIndex = groupIndex + landGroupIndex * 6;
      groups[baseIndex + 0] = landStart;
      groups[baseIndex + 1] = landCount;
    }
    if (waterStart != -1) {
      const int baseIndex = groupIndex + waterGroupIndex * 6;
      groups[baseIndex + 2] = waterStart;
      groups[baseIndex + 3] = waterCount;
    }
    if (lavaStart != -1) {
      const int baseIndex = groupIndex + lavaGroupIndex * 6;
      groups[baseIndex + 4] = lavaStart;
      groups[baseIndex + 5] = lavaCount;
    }

    groupIndex += NUM_RENDER_GROUPS * 6;
  }
  return groupIndex;
};
/* const _getObjectsCull = (hmdPosition, projectionMatrix, matrixWorldInverse) => {
  localFrustum.setFromMatrix(localMatrix.fromArray(projectionMatrix).multiply(localMatrix2.fromArray(matrixWorldInverse)));

  for (const index in zde.chunks) {
    const chunk = zde.chunks[index];

    if (chunk && chunk[objectsDecorationsSymbol]) {
      const {renderSpec} = chunk;

      renderSpec.groups.fill(-1);
      let groupIndex = 0;
      let start = -1;
      let count = 0;
      for (let i = 0; i < NUM_CHUNKS_HEIGHT; i++) {
        const trackedObjectChunkMesh = renderSpec.array[i];
        if (localFrustum.intersectsSphere(trackedObjectChunkMesh.boundingSphere)) {
          if (start === -1 && trackedObjectChunkMesh.indexRange.count > 0) {
            start = trackedObjectChunkMesh.indexRange.start;
          }
          count += trackedObjectChunkMesh.indexRange.count;
        } else {
          if (start !== -1) {
            const baseIndex = groupIndex * 2;
            renderSpec.groups[baseIndex + 0] = start;
            renderSpec.groups[baseIndex + 1] = count;
            groupIndex++;
            start = -1;
            count = 0;
          }
        }
      }
      if (start !== -1) {
        const baseIndex = groupIndex * 2;
        renderSpec.groups[baseIndex + 0] = start;
        renderSpec.groups[baseIndex + 1] = count;
      }
    }
  }

  return zde.chunks;
} */
