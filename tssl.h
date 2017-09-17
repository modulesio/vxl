#ifndef TSSL_H
#define TSSL_H

#include <node.h>

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Number;
using v8::Value;
using v8::Array;
using v8::ArrayBuffer;
using v8::Float32Array;
using v8::Uint32Array;
using v8::Uint8Array;

void tesselate(unsigned int *voxels, Local<Object> &blockTypes, int dims[3], unsigned char *transparentVoxels, unsigned char *translucentVoxels, float *faceUvs, float *shift, unsigned int numPositions, float *positions, float *uvs, unsigned char *ssaos, float *frames, float *objectIndices, unsigned int *indices, unsigned int &positionIndex, unsigned int &uvIndex, unsigned int &ssaoIndex, unsigned int &frameIndex, unsigned int &objectIndexIndex, unsigned int &indexIndex);

#endif
