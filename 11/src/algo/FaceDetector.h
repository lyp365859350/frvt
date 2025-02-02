#ifndef FACEDETECTOR_H_
#define FACEDETECTOR_H_

#include "Rect.h"
#include "ImageData.h"

namespace FRVT_11 {
    class FaceDetector {
public:
    virtual ~FaceDetector() {}

    virtual std::vector<Rect> Detect(const ImageData &image) const = 0;
};
}

#endif /* FACEDETECTOR_H_ */
