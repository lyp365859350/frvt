#include "SphereFaceRecognizer.h"

#include <algorithm>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "../algo/TimeMeasurement.h"

using namespace FRVT_11;

cv::Mat
CropImage(const cv::Mat& image, const std::vector<int>& landmarks)
{
    std::vector<int> xPoints;
    std::vector<int> yPoints;
    for (int i = 0; i < 10; i += 2) {
        xPoints.push_back(landmarks[i]);
        yPoints.push_back(landmarks[i+1]);
    }

    int xMin = *std::min_element(xPoints.begin(), xPoints.end());
    int xMax = *std::max_element(xPoints.begin(), xPoints.end());
    int yMin = *std::min_element(yPoints.begin(), yPoints.end());
    int yMax = *std::max_element(yPoints.begin(), yPoints.end());
    
    int w = (xMax - xMin);
    int h = (yMax - yMin);
    
    int x1 = xMin - int(w * 0.75);
    int x2 = xMax + int(w * 0.75);
    int y1 = yMin - int(h * 0.75);
    int y2 = yMax + int(h * 0.75);

    x1 = std::max(0, x1);
    x2 = std::min(image.cols, x2);
    y1 = std::max(0, y1);
    y2 = std::min(image.rows, y2);

    cv::Mat cropped = image(cv::Range(y1, y2), cv::Range(x1, x2));

    return cropped;
}

cv::Mat
NormalizeImage(const cv::Mat& image, const std::vector<int>& landmarks)
{
    // crop
    cv::Mat cropped = CropImage(image, landmarks);

    // To gray scale
    cv::Mat gray;
    cv::cvtColor(cropped, gray, cv::COLOR_RGB2GRAY);

    // resize
    cv::resize(gray, gray, cv::Size(128, 128), 0, 0, cv::INTER_LINEAR);

    // normalized
    gray.convertTo(gray, CV_32FC1);
    gray /= 255;
    gray -= 0.5;

    return gray;
}

SphereFaceRecognizer::SphereFaceRecognizer(const std::string &configDir)
{
    std::string sphereModelPath = configDir + "/fa_108_33-125000"; // sphereface-sphereface_108_cosineface_nist_bbox_33-125000
    mTensorFlowInference = std::make_shared<TensorFlowInference>(TensorFlowInference(sphereModelPath, {"input"}, {"output_features"}));
}

SphereFaceRecognizer::~SphereFaceRecognizer() {}

std::vector<float>
SphereFaceRecognizer::Infer(const ImageData& imageData, const std::vector<int>& landmarks) const
{
    cv::Mat image(imageData.height, imageData.width, CV_8UC3, imageData.data.get());
    
    image = NormalizeImage(image, landmarks);

    // infer on original image
    auto output = mTensorFlowInference->Infer(image);
    float* output_features = static_cast<float*>(TF_TensorData(output[0].get()));
    cv::Mat featuresMat_1(512, 1, CV_32F, output_features);

    // infer on flipped image
    cv::flip(image, image, 1);
    auto output_2 = mTensorFlowInference->Infer(image);
    float* output_features_2 = static_cast<float*>(TF_TensorData(output_2[0].get()));
    cv::Mat featuresMat_2(512, 1, CV_32F, output_features_2);

    // convert to vector (function should be changed to return cv::Mat)
    std::vector<float> features(1024);
    for (int i = 0; i < 512; ++i) {
        features[i] = featuresMat_1.at<float>(i, 0);
        features[i+512] = featuresMat_2.at<float>(i, 0);
    }

    return features;
}
