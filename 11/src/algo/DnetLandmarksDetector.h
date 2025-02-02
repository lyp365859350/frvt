#ifndef DNETLANDMARKSDETECTOR_H_
#define DNETLANDMARKSDETECTOR_H_

#include "LandmarksDetector.h"
#include "TensorFlowInference.h"

namespace FRVT_11 {
    class DnetLandmarksDetector : public LandmarksDetector {
public:
    DnetLandmarksDetector(const std::string &configDir);
    ~DnetLandmarksDetector() override;

    virtual std::vector<int> Detect(const ImageData& imageData, const Rect &face) const override;

private:
    std::vector<int> DoDetect(cv::Mat& image, const Rect &face) const;
    double CalcFlippedLandmarksDistance(const ImageData& imageData, const Rect &face, const std::vector<int>& landmarks) const;

    std::shared_ptr<TensorFlowInference> mTensorFlowInference;
};
}

#endif /* DNETLANDMARKSDETECTOR_H_ */
