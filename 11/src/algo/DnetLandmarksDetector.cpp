#include "DnetLandmarksDetector.h"

#include <iostream>
#include <cmath>
#include <algorithm>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

using namespace FRVT_11;

std::string DNET_MODEL_NAME = "/dnet_tffd_006";
int INPUT_SIZE = 64;
double MAX_FLIP_LANDMARKS_DISTANCE = 10000;

struct ImageCrop
{
    ImageCrop(cv::Mat& image, const Rect& rect)
    {
        // Rect to square

        h = rect.y2 - rect.y1;
        w = rect.x2 - rect.x1;
        n = std::max(h, w);
        int cropX = rect.x1 + w*0.5 - n*0.5;
        int cropY = rect.y1 + h*0.5 - n*0.5;
        Rect cropBox(cropX, cropY, cropX + n, cropY + n, rect.score);

        // Compute crop borders

        img_xbegin = int(round(cropBox[0]));
        img_ybegin = int(round(cropBox[1]));
        img_xend = int(round(cropBox[2])) + 1;
        img_yend = int(round(cropBox[3])) + 1;

        face_width  = img_xend - img_xbegin;
        face_height = img_yend - img_ybegin;

        dest_xbegin = 0;
        dest_ybegin = 0;
        dest_xend = face_width;
        dest_yend = face_height;

        int img_width = image.cols;
        int img_height = image.rows;

        if (img_xend > img_width) {
            dest_xend = face_width - (img_xend - img_width);
            img_xend = img_width;
        }
        if (img_yend > img_height) {
            dest_yend = face_height - (img_yend - img_height);
            img_yend = img_height;
        }
        if (img_xbegin < 0) {
            dest_xbegin = -img_xbegin;
            img_xbegin = 0;
        }
        if (img_ybegin < 0) {
            dest_ybegin = -img_ybegin;
            img_ybegin = 0;
        }

        cv::Mat cropFromOrig = image(cv::Range(img_ybegin, img_yend), cv::Range(img_xbegin, img_xend));
        cv::Mat cropped_img = cv::Mat::zeros(face_height, face_width, CV_8UC3);
        cv::Rect roiInNew(dest_xbegin, dest_ybegin, dest_xend - dest_xbegin, dest_yend - dest_ybegin);
        cropFromOrig.copyTo(cropped_img(roiInNew));
        image = cropped_img;

        cv::resize(image, image, cv::Size(INPUT_SIZE, INPUT_SIZE), 0, 0, cv::INTER_LINEAR);

        croppedImage = image;
    }

    int h;
    int w;
    int n;

    int img_xbegin;
    int img_ybegin;
    int img_xend;
    int img_yend;

    int face_width;
    int face_height;

    int dest_xbegin;
    int dest_ybegin;
    int dest_xend;
    int dest_yend;

    cv::Mat croppedImage;
};

ImageCrop
CropImage(cv::Mat& image, const Rect& rect)
{
    return ImageCrop(image, rect);
}

cv::Mat
NormalizeImage(cv::Mat& image)
{
    // To gray scale
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_RGB2GRAY);

    // normalized
    gray.convertTo(gray, CV_32FC1);
    gray -= 127.5;
    gray *= 0.0078125;

    return gray;
}

std::vector<int>
AdjustLandmarks(const ImageCrop& imageCrop, const float* landmarks)
{
    float cropH = imageCrop.img_yend - imageCrop.img_ybegin;
    float cropW = imageCrop.img_xend - imageCrop.img_xbegin;
    
    float ratioH = cropH / INPUT_SIZE;
    float ratioW = cropW / INPUT_SIZE;

    std::vector<float> adjustedLandmarks = {
        (landmarks[26] + landmarks[27] + landmarks[29] + landmarks[30]) / 4,
        (landmarks[26+43] + landmarks[27+43] + landmarks[29+43] + landmarks[30+43]) / 4,
        (landmarks[20] + landmarks[21] + landmarks[23] + landmarks[24]) / 4,
        (landmarks[20+43] + landmarks[21+43] + landmarks[23+43] + landmarks[24+43]) / 4,
        landmarks[13], landmarks[13+43],
        landmarks[37], landmarks[37+43],
        landmarks[31], landmarks[31+43]
    };

    for (int i = 0; i < 10; i = i + 2) {
        adjustedLandmarks[i] = adjustedLandmarks[i] * INPUT_SIZE * ratioW + imageCrop.img_xbegin;
        adjustedLandmarks[i+1] = adjustedLandmarks[i+1] * INPUT_SIZE * ratioH + imageCrop.img_ybegin;
    }

    std::vector<int> result(10);
    for (int i = 0; i < 10; ++i) result[i] = adjustedLandmarks[i];

    return result;
}

std::vector<int>
FlipLandmarks(const std::vector<int>& landmarks, int w)
{
    return {
        w - landmarks[2], landmarks[3],
        w - landmarks[0], landmarks[1],
        w - landmarks[4], landmarks[5],
        w - landmarks[8], landmarks[9],
        w - landmarks[6], landmarks[7]
    };
}

DnetLandmarksDetector::DnetLandmarksDetector(const std::string &configDir)
{
    std::string modelPath = configDir + DNET_MODEL_NAME;

    mTensorFlowInference = std::make_shared<TensorFlowInference>(TensorFlowInference(
        modelPath, {"d_net_input"}, {"lm_output/BiasAdd"})
    );
}

DnetLandmarksDetector::~DnetLandmarksDetector() {}

std::vector<int>
DnetLandmarksDetector::Detect(const ImageData& imageData, const Rect &face) const
{
    cv::Mat image(imageData.height, imageData.width, CV_8UC3, imageData.data.get());
    auto landmarks = DoDetect(image, face);

    auto distance = CalcFlippedLandmarksDistance(imageData, face, landmarks);

    if (distance > MAX_FLIP_LANDMARKS_DISTANCE)
    {
        landmarks.clear();
    }

    return landmarks;
}

std::vector<int>
DnetLandmarksDetector::DoDetect(cv::Mat& image, const Rect &face) const
{
    // Prepare image

    ImageCrop imageCrop = CropImage(image, face);

    image = NormalizeImage(imageCrop.croppedImage);

    // Perform inference
    auto output = mTensorFlowInference->Infer(image);
    float* output_landmarks = static_cast<float*>(TF_TensorData(output[0].get()));

    // Process output

    std::vector<int> landmarks = AdjustLandmarks(imageCrop, output_landmarks);

    return landmarks;
}

double
DnetLandmarksDetector::CalcFlippedLandmarksDistance(const ImageData& imageData, const Rect &face, const std::vector<int>& landmarks) const
{
    if (MAX_FLIP_LANDMARKS_DISTANCE == 10000) return 0;

    cv::Mat image(imageData.height, imageData.width, CV_8UC3, imageData.data.get());
    cv::Mat flippedImage;
    cv::flip(image, flippedImage, 1);

    Rect flippedFace(image.cols - face.x1, face.y1, image.cols - face.x2, face.y2, face.score);

    auto flippedLandmarks = DoDetect(flippedImage, flippedFace);
    
    flippedLandmarks = FlipLandmarks(flippedLandmarks, imageData.width);

    double distance = 0;
    for (int i = 0; i < 10; ++i) distance += std::pow(landmarks[i] - flippedLandmarks[i], 2);
    distance = std::sqrt(distance);

    return distance;
}
